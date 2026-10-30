// Editor seed collection and packaged seed-bank playback. No UnrealEd module dependency.

#include "Enviornment/MazeGenerator.h"
#include "Enviornment/MazeActors.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"

namespace
{
    FString EscapeSeedField(const FString& Text)
    {
        return Text.Replace(TEXT("%"), TEXT("%25"), ESearchCase::CaseSensitive)
            .Replace(TEXT("|"), TEXT("%7C"), ESearchCase::CaseSensitive)
            .Replace(TEXT("\r"), TEXT("%0D"), ESearchCase::CaseSensitive)
            .Replace(TEXT("\n"), TEXT("%0A"), ESearchCase::CaseSensitive);
    }

    FString UnescapeSeedField(const FString& Text)
    {
        return Text.Replace(TEXT("%0A"), TEXT("\n"), ESearchCase::CaseSensitive)
            .Replace(TEXT("%0D"), TEXT("\r"), ESearchCase::CaseSensitive)
            .Replace(TEXT("%7C"), TEXT("|"), ESearchCase::CaseSensitive)
            .Replace(TEXT("%25"), TEXT("%"), ESearchCase::CaseSensitive);
    }

    FString SeedRoomLabel(const FString& ClassPath)
    {
        if (ClassPath.IsEmpty()) { return FString(); }
        TArray<FString> Parts;
        ClassPath.ParseIntoArray(Parts, TEXT("/"), false);
        const FString Leaf = Parts[Parts.Num() - 1];
        Leaf.ParseIntoArray(Parts, TEXT("."), false);
        return Parts[0];
    }

    FString SeedClassPath(const UClass* Class)
    {
        return Class ? Class->GetPathName() : FString();
    }
}

FString AMazeGenerator::BuildSeedBankProfile(AMazeActors* Room, FName DoorTag, FName EntranceDoorTag) const
{
    UArrowComponent* Door = FindTaggedConnection(Room, DoorTag);
    TArray<FBox> Bounds;
    if (!IsValid(Room) || !IsValid(Door) || !GetFootprints(Room, Bounds))
    {
        return FString();
    }

    // World position is deliberately excluded: later start rooms move through the level.
    const FTransform Anchor(FRotator(0.0f, Door->GetForwardVector().Rotation().Yaw, 0.0f), Door->GetComponentLocation());
    FString Settings = TEXT("MadCardSeedReplay=4\n");
    const auto AddInt = [&Settings](const TCHAR* Name, int32 Value)
        { Settings += FString::Printf(TEXT("%s=%d\n"), Name, Value); };
    const auto AddFloat = [&Settings](const TCHAR* Name, float Value)
        { Settings += FString::Printf(TEXT("%s=%.6f\n"), Name, Value); };
    const auto AddClass = [&Settings](const TCHAR* Name, const UClass* Class)
        { Settings += FString::Printf(TEXT("%s=%s\n"), Name, *SeedClassPath(Class)); };
    for (const FName& Id : GetSeedFilePuzzleIds()) { Settings += TEXT("SeedFile=") + Id.ToString() + TEXT("\n"); }
    AddClass(TEXT("StartRoom"), Room->GetClass());
    Settings += TEXT("Door=") + DoorTag.ToString() + TEXT("\nIncoming=") + EntranceDoorTag.ToString() + TEXT("\n");

    TArray<FString> Geometry;
    // Hash the actual box corners in doorway space, before world-axis expansion.
    // Moving or rotating a later start room must not create another setup profile.
    TArray<UBoxComponent*> ProfileBounds;
    Room->GetComponents<UBoxComponent>(ProfileBounds, true);
    for (UBoxComponent* Box : ProfileBounds)
    {
        if (!IsValid(Box) || !Box->ComponentHasTag(TEXT("MazeBounds"))) { continue; }
        const FVector Extent = Box->GetUnscaledBoxExtent();
        const FTransform BoundsTransform = Box->GetComponentTransform();
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector Local = Anchor.InverseTransformPosition(BoundsTransform.TransformPosition(FVector(
                (Corner & 1) ? Extent.X : -Extent.X,
                (Corner & 2) ? Extent.Y : -Extent.Y,
                (Corner & 4) ? Extent.Z : -Extent.Z)));
            Geometry.Add(FString::Printf(TEXT("B=%.1f,%.1f,%.1f"),
                FMath::Abs(Local.X) < 0.05 ? 0.0 : Local.X,
                FMath::Abs(Local.Y) < 0.05 ? 0.0 : Local.Y,
                FMath::Abs(Local.Z) < 0.05 ? 0.0 : Local.Z));
        }
    }
    for (UArrowComponent* Arrow : GetSortedConnections(Room))
    {
        const FVector Local = Anchor.InverseTransformPosition(Arrow->GetComponentLocation());
        const FVector Forward = Anchor.InverseTransformVectorNoScale(Arrow->GetForwardVector());
        Geometry.Add(FString::Printf(TEXT("A=%s:%.1f,%.1f,%.1f:%.3f,%.3f,%.3f"), *Arrow->GetName(),
            FMath::Abs(Local.X) < 0.05 ? 0.0 : Local.X, FMath::Abs(Local.Y) < 0.05 ? 0.0 : Local.Y,
            FMath::Abs(Local.Z) < 0.05 ? 0.0 : Local.Z, FMath::Abs(Forward.X) < 0.0005 ? 0.0 : Forward.X,
            FMath::Abs(Forward.Y) < 0.0005 ? 0.0 : Forward.Y, FMath::Abs(Forward.Z) < 0.0005 ? 0.0 : Forward.Z));
    }
    Geometry.Sort();
    for (const FString& Line : Geometry) { Settings += Line + TEXT("\n"); }

    AddInt(TEXT("Hallways"), MaxHallwaysForward);
    AddInt(TEXT("MaxRooms"), MaxRouteRooms);
    AddInt(TEXT("RoomSpacing"), HallwaysBetweenRooms);
    AddInt(TEXT("RandomRooms"), bRandomizeRouteRooms);
    AddInt(TEXT("BranchRooms"), bAllowRoomsOnBranches);
    AddInt(TEXT("MaxBranchRooms"), MaxBranchRooms);
    AddInt(TEXT("Junctions"), bAllowJunctions);
    AddInt(TEXT("MinJunctionSpacing"), MinHallwaysBetweenJunctions);
    AddInt(TEXT("MaxJunctionSpacing"), MaxHallwaysBetweenJunctions);
    AddInt(TEXT("BranchHallways"), MaxBranchHallways);
    AddInt(TEXT("BranchEnemies"), bAllowEnemyHallwaysOnBranches);
    AddInt(TEXT("EnemySpacing"), MinHallwaysBetweenEnemyHallways);
    AddInt(TEXT("BeforePuzzle"), MinHallwaysBeforePuzzle);
    AddInt(TEXT("AfterPuzzle"), MinHallwaysAfterPuzzle);
    AddInt(TEXT("ExitHallways"), HallwaysAfterPuzzle);
    AddInt(TEXT("DebugButtons"), bSpawnAllPuzzleButtons);
    AddInt(TEXT("DebugAngels"), bSpawnAllPuzzleWeepingAngels);
    AddFloat(TEXT("Seams"), BoundsSeamTolerance);
    AddFloat(TEXT("ConnectionTolerance"), ConnectionTolerance);
    AddFloat(TEXT("Clearance"), ConnectionClearanceRadius);
    AddFloat(TEXT("NavWidth"), NavBoundsWidthMultiplier);
    AddFloat(TEXT("NavLength"), NavBoundsLengthMultiplier);
    AddFloat(TEXT("NavPadding"), NavBoundsHorizontalPadding);
    AddFloat(TEXT("NavHeightPadding"), NavBoundsHeightPadding);
    AddFloat(TEXT("AcceptedSeconds"), MaxAcceptedSeedSeconds);
    AddClass(TEXT("EndCap"), EndCapClass.Get());
    AddClass(TEXT("EndRoom"), EndRoomClass.Get());
    AddClass(TEXT("NextStart"), NextStartRoomClass.Get());
    for (const TSubclassOf<AMazeActors>& Class : CorridorClasses) { AddClass(TEXT("Corridor"), Class.Get()); }
    for (const TSubclassOf<AMazeActors>& Class : EnemyCorridorClasses) { AddClass(TEXT("EnemyCorridor"), Class.Get()); }
    for (const TSubclassOf<AMazeActors>& Class : RouteRoomClasses) { AddClass(TEXT("RouteRoom"), Class.Get()); }
    TArray<FName> ProfileTags;
    EnemyClassesByTag.GetKeys(ProfileTags);
    ProfileTags.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    for (const FName& Tag : ProfileTags)
    {
        Settings += TEXT("Enemy=") + Tag.ToString() + TEXT(":") + SeedClassPath(EnemyClassesByTag.FindRef(Tag).Get()) + TEXT("\n");
    }
    ProfileTags.Reset(); EnemyHallwayCounts.GetKeys(ProfileTags);
    ProfileTags.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    for (const FName& Tag : ProfileTags) { Settings += FString::Printf(TEXT("Count=%s:%d\n"), *Tag.ToString(), EnemyHallwayCounts.FindRef(Tag)); }
    ProfileTags.Reset(); PuzzleRoomsById.GetKeys(ProfileTags);
    ProfileTags.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    for (const FName& Tag : ProfileTags) { Settings += TEXT("Puzzle=") + Tag.ToString() + TEXT(":") + SeedClassPath(PuzzleRoomsById.FindRef(Tag).Get()) + TEXT("\n"); }
    Settings += TEXT("Tags=") + ChaserEnemyTag.ToString() + TEXT("|") + ChaserSpawnTriggerTag.ToString()
        + TEXT("|") + WeepingAngelEnemyTag.ToString() + TEXT("|") + PuzzleRoomExitTag.ToString()
        + TEXT("|") + EndRoomExitTag.ToString() + TEXT("|") + NextRoomEntranceDoorTag.ToString()
        + TEXT("|") + MazeEndDoorTag.ToString() + TEXT("\n");
    return FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Settings));
}

TArray<FName> AMazeGenerator::GetSeedFilePuzzleIds() const
{
    TArray<FName> PuzzleIds = SeedFilePuzzleIds;
    if (PuzzleIds.IsEmpty())
    {
        PuzzleRoomsById.GetKeys(PuzzleIds);
        PuzzleIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    }
    if (PuzzleIds.Num() != 3) { return {}; }
    for (int32 Index = 0; Index < PuzzleIds.Num(); ++Index)
    {
        if (PuzzleIds[Index].IsNone() || !PuzzleRoomsById.Contains(PuzzleIds[Index])) { return {}; }
        for (int32 Other = 0; Other < Index; ++Other)
        {
            if (PuzzleIds[Other] == PuzzleIds[Index]) { return {}; }
        }
    }
    return PuzzleIds;
}

bool AMazeGenerator::SetSeedBankPuzzleFile(FName PuzzleId)
{
    const TArray<FName> PuzzleIds = GetSeedFilePuzzleIds();
    for (int32 Index = 0; Index < PuzzleIds.Num(); ++Index)
    {
        if (PuzzleIds[Index] != PuzzleId) { continue; }
        SeedBankFilePuzzleId = PuzzleId;
        CurrentSeedBankFile = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MazeSeeds"),
            FString::Printf(TEXT("gen %d.txt"), Index + 1));
        return true;
    }
    UE_LOG(LogTemp, Error, TEXT("Maze seeds: Assign three different SeedFilePuzzleIds matching PuzzleRoomsById, or leave the list empty for alphabetical order."));
    return false;
}

bool AMazeGenerator::LoadSeedBankFile(bool bRequired)
{
    SeedBankEntries.Reset();
    const TArray<FName> PuzzleIds = GetSeedFilePuzzleIds();
    if (PuzzleIds.Num() != 3) { return false; }
    for (const FName& Id : PuzzleIds)
    {
        if (!SetSeedBankPuzzleFile(Id)) { SeedBankEntries.Reset(); return false; }
        if (!IFileManager::Get().FileExists(*CurrentSeedBankFile)) { continue; }
        if (!LoadPuzzleSeedBankFile(bRequired)) { SeedBankEntries.Reset(); return false; }
    }
    return true;
}

bool AMazeGenerator::LoadPuzzleSeedBankFile(bool bRequired)
{
    FString Text;
    if (IFileManager::Get().FileSize(*CurrentSeedBankFile) > 5 * 1024 * 1024
        || !FFileHelper::LoadFileToString(Text, *CurrentSeedBankFile))
    {
        if (bRequired) { UE_LOG(LogTemp, Error, TEXT("Maze seeds: Could not read %s."), *CurrentSeedBankFile); }
        return false;
    }
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, true);
    TArray<FString> Header;
    if (!Lines.IsEmpty()) { Lines[0].ParseIntoArray(Header, TEXT("|"), false); }
    int32 ExpectedCount = 0;
    if (Header.Num() != 5 || Header[0] != TEXT("MAD_CARD_SEEDS") || Header[1] != TEXT("4")
        || Header[2] != SeedBankFilePuzzleId.ToString() || !LexTryParseString(ExpectedCount, *Header[3])
        || ExpectedCount < 0 || ExpectedCount > 1000 || Lines.Num() != ExpectedCount + 1)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: Invalid, incomplete, or incompatible file %s. It was not overwritten."), *CurrentSeedBankFile);
        return false;
    }
    FString Body;
    for (int32 Index = 1; Index < Lines.Num(); ++Index) { Body += Lines[Index] + TEXT("\n"); }
    if (Header[4] != FString::Printf(TEXT("%08X"), FCrc::StrCrc32(*Body)))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: Checksum mismatch in %s. It was not overwritten."), *CurrentSeedBankFile);
        return false;
    }
    for (int32 Index = 1; Index < Lines.Num(); ++Index)
    {
        TArray<FString> Fields;
        Lines[Index].ParseIntoArray(Fields, TEXT("|"), false);
        FSeedBankEntry Entry;
        if (Fields.Num() != 6 || !LexTryParseString(Entry.Seed, *Fields[0]) || Entry.Seed < 1
            || !LexTryParseString(Entry.PreviousRoomCount, *Fields[3]) || Entry.PreviousRoomCount < 0
            || Entry.PreviousRoomCount > 2 || !LexTryParseString(Entry.Seconds, *Fields[4])
            || !FMath::IsFinite(Entry.Seconds) || Entry.Seconds < 0.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("Maze seeds: Invalid entry at line %d in %s."), Index + 1, *CurrentSeedBankFile);
            SeedBankEntries.Reset(); return false;
        }
        Entry.Profile = Fields[5];
        if (Entry.Profile.IsEmpty()) { SeedBankEntries.Reset(); return false; }
        Entry.PuzzleId = FName(*UnescapeSeedField(Fields[1]));
        Entry.PreviousRoomClass = UnescapeSeedField(Fields[2]);
        const bool bKnownProfile = Entry.Profile == SeedBankProfile || SeedCollectionSetups.ContainsByPredicate(
            [&Entry](const FSeedCollectionSetup& Setup) { return Setup.Profile == Entry.Profile; });
        if (bKnownProfile && !Entry.PreviousRoomClass.IsEmpty())
        {
            FString ResolvedClass;
            int32 Matches = 0;
            for (const TSubclassOf<AMazeActors>& Class : RouteRoomClasses)
            {
                const FString ClassPath = SeedClassPath(Class.Get());
                if (Class && (Entry.PreviousRoomClass == ClassPath || Entry.PreviousRoomClass == SeedRoomLabel(ClassPath)))
                {
                    if (ResolvedClass != ClassPath) { ResolvedClass = ClassPath; ++Matches; }
                }
            }
            if (Matches != 1)
            {
                UE_LOG(LogTemp, Error, TEXT("Maze seeds: Unknown or ambiguous room label in %s. Use unique route-room Blueprint names or recollect with full paths."), *CurrentSeedBankFile);
                SeedBankEntries.Reset(); return false;
            }
            Entry.PreviousRoomClass = ResolvedClass;
        }
        if (Entry.PuzzleId != SeedBankFilePuzzleId || !PuzzleRoomsById.Contains(Entry.PuzzleId)
            || (Entry.PreviousRoomClass.IsEmpty() != (Entry.PreviousRoomCount == 0))
            || (bKnownProfile && !Entry.PreviousRoomClass.IsEmpty() && !RouteRoomClasses.ContainsByPredicate([&Entry](const TSubclassOf<AMazeActors>& Class)
                { return SeedClassPath(Class.Get()) == Entry.PreviousRoomClass; }))
            || SeedBankEntries.ContainsByPredicate([&Entry](const FSeedBankEntry& Other)
                { return Entry.Profile == Other.Profile && Entry.Seed == Other.Seed && Entry.PuzzleId == Other.PuzzleId
                && Entry.PreviousRoomClass == Other.PreviousRoomClass && Entry.PreviousRoomCount == Other.PreviousRoomCount; }))
        {
            UE_LOG(LogTemp, Error, TEXT("Maze seeds: Unknown puzzle, invalid history, or duplicate entry in %s."), *CurrentSeedBankFile);
            SeedBankEntries.Reset(); return false;
        }
        SeedBankEntries.Add(Entry);
    }
    return true;
}

bool AMazeGenerator::SaveSeedBankFile()
{
    return bSeedCollectionRunning && SetSeedBankPuzzleFile(ActivePuzzleId) && WriteSeedBankFile();
}

bool AMazeGenerator::WriteSeedBankFile()
{
#if WITH_EDITOR
    if ((!bSeedCollectionRunning && !bReplaceSeedBankOnCollection)
        || !GetWorld() || GetWorld()->WorldType != EWorldType::PIE) {
        return false;
    }
    FString Body;
    int32 FileEntryCount = 0;
    for (const FSeedBankEntry& Entry : SeedBankEntries)
    {
        if (Entry.PuzzleId != SeedBankFilePuzzleId) { continue; }
        ++FileEntryCount;
        FString RoomField = Entry.PreviousRoomClass;
        const bool bKnownProfile = Entry.Profile == SeedBankProfile || SeedCollectionSetups.ContainsByPredicate(
            [&Entry](const FSeedCollectionSetup& Setup) { return Setup.Profile == Entry.Profile; });
        if (bKnownProfile && !RoomField.IsEmpty())
        {
            const FString Label = SeedRoomLabel(RoomField);
            bool bUniqueLabel = true;
            for (const TSubclassOf<AMazeActors>& Class : RouteRoomClasses)
            {
                const FString ClassPath = SeedClassPath(Class.Get());
                if (Class && ClassPath != RoomField && SeedRoomLabel(ClassPath) == Label) { bUniqueLabel = false; break; }
            }
            if (bUniqueLabel) { RoomField = Label; }
        }
        Body += FString::Printf(TEXT("%d|%s|%s|%d|%.6f|%s\n"), Entry.Seed,
            *EscapeSeedField(Entry.PuzzleId.ToString()), *EscapeSeedField(RoomField), Entry.PreviousRoomCount, Entry.Seconds, *Entry.Profile);
    }
    if (FileEntryCount > FMath::Clamp(MaxSavedSeedsPerPuzzle, 16, 1000)) { return false; }
    const FString Text = FString::Printf(TEXT("MAD_CARD_SEEDS|4|%s|%d|%08X\n"),
        *SeedBankFilePuzzleId.ToString(), FileEntryCount, FCrc::StrCrc32(*Body)) + Body;
    const FString Temporary = CurrentSeedBankFile + TEXT(".tmp");
    IFileManager& Files = IFileManager::Get();
    if (!Files.MakeDirectory(*FPaths::GetPath(CurrentSeedBankFile), true)
        || !FFileHelper::SaveStringToFile(Text, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !Files.Move(*CurrentSeedBankFile, *Temporary, true, false, false, true))
    {
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: Could not save %s. Check folder permissions/source-control read-only files. Collection stopped."), *CurrentSeedBankFile);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool AMazeGenerator::PrepareSeedCollectionSetups(AMazeActors* Room, FName DoorTag)
{
#if WITH_EDITOR
    SeedSetupCount = 0;
    CoveredSeedBuckets = 0;
    RequiredSeedBuckets = 0;
    if (!GetWorld() || GetWorld()->WorldType != EWorldType::PIE || Room != InitialStartRoom.Get()
        || !IsValid(Room) || !CurrentRoomEntranceTag.IsNone() || RoomsCompleted != 0
        || DoorTags.Num() != 3 || DoorTag != DoorTags[0] || !NextStartRoomClass
        || NextRoomEntranceDoorTag.IsNone() || DoorTags.Contains(NextRoomEntranceDoorTag))
    {
        SeedCollectionStatus = TEXT("Start full-loop collection from Door1 in the original start room in a fresh PIE session.");
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: %s"), *SeedCollectionStatus);
        return false;
    }

    SeedCollectionSourceRoom = Room;
    SeedCollectionSourceDoorTag = DoorTag;
    SeedCollectionSourceEntranceTag = CurrentRoomEntranceTag;
    SeedCollectionOriginalRoomClass = LastRouteRoomClass;
    SeedCollectionOriginalRoomCount = ConsecutiveRouteRooms;
    SeedCollectionOriginalPuzzleId = ActivePuzzleId;
    SeedCollectionOriginalPuzzleClass = ActivePuzzleRoomClass;
    SeedCollectionOriginalRandomStream = RandomStream;
    SeedCollectionOriginalNavTransform = IsValid(MazeNavBounds) ? MazeNavBounds->GetActorTransform() : FTransform::Identity;
    SeedCollectionSetups.Reset();

    FSeedCollectionSetup First;
    First.Room = Room;
    First.DoorTag = DoorTags[0];
    First.Profile = BuildSeedBankProfile(Room, First.DoorTag, NAME_None);
    if (First.Profile.IsEmpty()) { return false; }
    SeedCollectionSetups.Add(First);

    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // Keep temporary next-room tests away from the live room and player.
    SeedCollectionPreviewRoom = GetWorld()->SpawnActor<AMazeActors>(NextStartRoomClass,
        Room->GetActorLocation() + FVector(0.0, 0.0, 100000.0), FRotator::ZeroRotator, Parameters);
    if (!IsValid(SeedCollectionPreviewRoom) || !ValidateRoom(SeedCollectionPreviewRoom, NextRoomEntranceDoorTag))
    {
        SeedCollectionStatus = TEXT("NextStartRoomClass needs valid Door1/2/3, a separate incoming Door4, bounds, and RoomArrival before collection.");
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: %s"), *SeedCollectionStatus);
        return false;
    }
    TArray<USceneComponent*> PreviewComponents;
    SeedCollectionPreviewRoom->GetComponents<USceneComponent>(PreviewComponents, true);
    for (USceneComponent* Component : PreviewComponents)
    {
        if (IsValid(Component)) { Component->SetMobility(EComponentMobility::Movable); }
    }

    for (const FName& Tag : DoorTags)
    {
        FSeedCollectionSetup Setup;
        Setup.Room = SeedCollectionPreviewRoom.Get();
        Setup.DoorTag = Tag;
        Setup.EntranceDoorTag = NextRoomEntranceDoorTag;
        Setup.Profile = BuildSeedBankProfile(SeedCollectionPreviewRoom, Tag, Setup.EntranceDoorTag);
        if (Setup.Profile.IsEmpty() || SeedCollectionSetups.ContainsByPredicate(
            [&Setup](const FSeedCollectionSetup& Other) { return Other.Profile == Setup.Profile; })) {
            return false;
        }
        SeedCollectionSetups.Add(Setup);
    }
    SeedSetupCount = SeedCollectionSetups.Num();
    return SeedSetupCount == 4;
#else
    return false;
#endif
}

bool AMazeGenerator::PrepareSeedCollectionBuckets()
{
    SeedCollectionBuckets.Reset();
    TArray<UClass*> HistoryClasses;
    for (const TSubclassOf<AMazeActors>& Class : RouteRoomClasses) { if (Class) { HistoryClasses.AddUnique(Class.Get()); } }
    HistoryClasses.Sort([](const UClass& A, const UClass& B) { return A.GetPathName() < B.GetPathName(); });
    const int32 HistoryVariants = 1 + HistoryClasses.Num() * 2;
    const int32 BucketsPerPuzzle = 1 + (SeedCollectionSetups.Num() - 1) * HistoryVariants;
    const int32 FileCap = FMath::Clamp(MaxSavedSeedsPerPuzzle, 16, 1000);
    if (SeedCollectionSetups.Num() != 4 || FileCap < BucketsPerPuzzle || PuzzleRoomsById.Num() != 3
        || bSpawnAllPuzzleButtons || bSpawnAllPuzzleWeepingAngels)
    {
        SeedCollectionStatus = FString::Printf(TEXT("Full-loop collection needs four setups, three puzzles, spawn-all debugging off, and at least %d seeds per puzzle file. Increase Max Saved Seeds Per Puzzle; 50 suits the Card/Trap setup."), BucketsPerPuzzle);
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: %s"), *SeedCollectionStatus);
        return false;
    }
    TArray<FName> PuzzleIds;
    PuzzleRoomsById.GetKeys(PuzzleIds);
    PuzzleIds.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    for (const FName& Id : PuzzleIds)
    {
        const int32 FirstBucket = SeedCollectionBuckets.Num();
        for (int32 SetupIndex = 0; SetupIndex < SeedCollectionSetups.Num(); ++SetupIndex)
        {
            // A fresh first room has no preceding route; later rooms can inherit every history.
            const int32 Variants = SetupIndex == 0 ? 1 : HistoryVariants;
            for (int32 Variant = 0; Variant < Variants; ++Variant)
            {
                FSeedCollectionBucket Bucket;
                Bucket.SetupIndex = SetupIndex;
                Bucket.PuzzleId = Id;
                if (Variant > 0)
                {
                    Bucket.PreviousRoomClass = HistoryClasses[(Variant - 1) / 2];
                    Bucket.PreviousRoomCount = 1 + (Variant - 1) % 2;
                }
                const FString ClassPath = SeedClassPath(Bucket.PreviousRoomClass.Get());
                for (const FSeedBankEntry& Entry : SeedBankEntries)
                {
                    if (Entry.Profile == SeedCollectionSetups[SetupIndex].Profile && Entry.PuzzleId == Id
                        && Entry.PreviousRoomClass == ClassPath && Entry.PreviousRoomCount == Bucket.PreviousRoomCount) {
                        ++Bucket.Saved;
                    }
                }
                Bucket.Target = FMath::Max(1, Bucket.Saved);
                SeedCollectionBuckets.Add(Bucket);
            }
        }
        int32 ExistingCount = 0;
        int32 CoveredCount = 0;
        int32 ReservedCount = 0;
        for (const FSeedBankEntry& Entry : SeedBankEntries) { if (Entry.PuzzleId == Id) { ++ExistingCount; } }
        for (int32 Index = FirstBucket; Index < SeedCollectionBuckets.Num(); ++Index)
        {
            CoveredCount += SeedCollectionBuckets[Index].Saved;
            ReservedCount += SeedCollectionBuckets[Index].Target;
        }
        int32 SlotsLeft = FileCap - (ExistingCount - CoveredCount) - ReservedCount;
        if (SlotsLeft < 0)
        {
            SeedCollectionStatus = TEXT("Existing entries leave insufficient space for every door/history. Enable Replace Seed Bank On Collection once to redistribute all three files, or increase the file cap. No files were changed.");
            UE_LOG(LogTemp, Error, TEXT("Maze seeds: %s"), *SeedCollectionStatus);
            return false;
        }
        // Balance the remaining capacity without deleting accepted entries from a resumed bank.
        while (SlotsLeft-- > 0)
        {
            int32 Smallest = FirstBucket;
            for (int32 Index = FirstBucket + 1; Index < SeedCollectionBuckets.Num(); ++Index)
            {
                if (SeedCollectionBuckets[Index].Target < SeedCollectionBuckets[Smallest].Target) { Smallest = Index; }
            }
            ++SeedCollectionBuckets[Smallest].Target;
        }
    }
    SavedSeedCount = SeedBankEntries.Num();
    TargetSavedSeedCount = FileCap * 3;
    UpdateSeedCollectionCoverage();
    return true;
}

void AMazeGenerator::UpdateSeedCollectionCoverage()
{
    RequiredSeedBuckets = SeedCollectionBuckets.Num();
    CoveredSeedBuckets = 0;
    for (const FSeedCollectionBucket& Bucket : SeedCollectionBuckets)
    {
        if (Bucket.Saved > 0) { ++CoveredSeedBuckets; }
    }
}

bool AMazeGenerator::PrepareSeedSession(AMazeActors* Room, FName DoorTag)
{
    ResetSeedSession();
    SeedBankProfile = BuildSeedBankProfile(Room, DoorTag, CurrentRoomEntranceTag);
    const TArray<FName> FilePuzzleIds = GetSeedFilePuzzleIds();
    if (FilePuzzleIds.Num() != 3) { return PuzzleRoomsById.IsEmpty() && !bRequireSavedSeeds; }
    if (!SetSeedBankPuzzleFile(ActivePuzzleId.IsNone() ? FilePuzzleIds[0] : ActivePuzzleId)) { return false; }
    bool bCollect = false;
#if WITH_EDITOR
    bCollect = (bCollectSeedsInEditor || bReplaceSeedBankOnCollection)
        && GetWorld() && GetWorld()->WorldType == EWorldType::PIE;
#endif
    if (SeedBankProfile.IsEmpty()) { return !bCollect && !bRequireSavedSeeds; }
    if (!bCollect && !bUseSavedSeeds) { return true; }
    const bool bReplace = bCollect && bReplaceSeedBankOnCollection;
    if (bCollect && !PrepareSeedCollectionSetups(Room, DoorTag)) { ResetSeedSession(); return false; }
    if (!bReplace)
    {
        if (bCollect)
        {
            if (!LoadSeedBankFile(true)) { ResetSeedSession(); return false; }
        }
        else if (IFileManager::Get().FileExists(*CurrentSeedBankFile) && !LoadPuzzleSeedBankFile(true)) { return false; }
    }

    if (bCollect)
    {
        if (!PrepareSeedCollectionBuckets()) { ResetSeedSession(); return false; }
        if (bReplace)
        {
            // Keep a valid empty header/checksum so an interrupted refill can resume normally.
            for (const FName& Id : FilePuzzleIds)
            {
                if (!SetSeedBankPuzzleFile(Id) || !WriteSeedBankFile())
                {
                    SeedCollectionStatus = TEXT("Replacement stopped; check the Output Log. Earlier puzzle files may already be cleared.");
                    ResetSeedSession();
                    return false;
                }
            }
            bReplaceSeedBankOnCollection = false;
            UE_LOG(LogTemp, Display, TEXT("Maze seeds: Cleared saved entries for %s. Collecting a replacement bank."), *CurrentSeedBankFile);
        }
        if (!SeedCollectionBuckets.ContainsByPredicate([](const FSeedCollectionBucket& B) { return B.Saved < B.Target; }))
        {
            SeedCollectionStatus = TEXT("All four door setups already reached their seed targets. Disable collection for normal play.");
            UE_LOG(LogTemp, Display, TEXT("Maze seeds: %s File: %s"), *SeedCollectionStatus, *CurrentSeedBankFile);
            ResetSeedSession();
            return false;
        }
        bSeedCollectionRunning = true;
        bSeedSessionPrepared = true;
        TestedSeedCount = 0;
        SeedCollectionBucketIndex = -1;
        NextSeedToTest = 1;
        for (const FSeedBankEntry& Entry : SeedBankEntries)
        {
            if (Entry.Seed >= NextSeedToTest && Entry.Seed < MAX_int32) { NextSeedToTest = Entry.Seed + 1; }
        }
        SeedCollectionStatus = TEXT("Collecting the first door and all three next-room doors; the live door stays closed.");
        UE_LOG(LogTemp, Display, TEXT("Maze seeds: Collecting %d/%d across all four door setups. File: %s"), SavedSeedCount, TargetSavedSeedCount, *CurrentSeedBankFile);
        return true;
    }

    const FString PreviousClass = SeedClassPath(LastRouteRoomClass.Get());
    const int32 PreviousCount = LastRouteRoomClass ? FMath::Clamp(ConsecutiveRouteRooms, 1, 2) : 0;
    RuntimeSeedBucketKey = SeedBankProfile + TEXT("|") + ActivePuzzleId.ToString() + TEXT("|") + PreviousClass + FString::Printf(TEXT("|%d"), PreviousCount);
    for (const FSeedBankEntry& Entry : SeedBankEntries)
    {
        if (Entry.Profile == SeedBankProfile && Entry.PuzzleId == ActivePuzzleId && Entry.PreviousRoomClass == PreviousClass && Entry.PreviousRoomCount == PreviousCount)
        {
            SavedSeedCandidates.Add(Entry.Seed);
        }
    }
    if (SavedSeedCandidates.Num() > 1)
    {
        if (const int32* Last = LastUsedBankSeeds.Find(RuntimeSeedBucketKey)) { SavedSeedCandidates.Remove(*Last); }
    }
    for (int32 Index = SavedSeedCandidates.Num() - 1; Index > 0; --Index)
    {
        SavedSeedCandidates.Swap(Index, RandomStream.RandRange(0, Index));
    }
    if (SavedSeedCandidates.IsEmpty() && bRequireSavedSeeds)
    {
        SeedCollectionStatus = TEXT("No approved seeds match this puzzle, door setup, and room history.");
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: %s Run full-loop collection from the first room's Door1 in PIE. Puzzle: %s. Room: %s. Door: %s. Incoming: %s. Profile: %s. History: %s x%d. File: %s"),
            *SeedCollectionStatus, *ActivePuzzleId.ToString(), *SeedClassPath(Room->GetClass()), *DoorTag.ToString(),
            *CurrentRoomEntranceTag.ToString(), *SeedBankProfile, *PreviousClass, PreviousCount, *CurrentSeedBankFile);
        return false;
    }
    bSeedSessionPrepared = true;
    SeedCollectionStatus = SavedSeedCandidates.IsEmpty() ? TEXT("No matching bank; normal generation enabled.") : TEXT("Using approved seeds.");
    return true;
}

bool AMazeGenerator::PrepareLayoutSeed()
{
    bSeedAttemptUsesBank = false;
    if (bSeedCollectionRunning)
    {
        const int32 Count = SeedCollectionBuckets.Num();
        int32 SetupToTest = INDEX_NONE;
        for (int32 SetupIndex = 0; SetupIndex < SeedCollectionSetups.Num(); ++SetupIndex)
        {
            if (SeedCollectionBuckets.ContainsByPredicate([SetupIndex](const FSeedCollectionBucket& Bucket)
                { return Bucket.SetupIndex == SetupIndex && Bucket.Saved < Bucket.Target; })) {
                SetupToTest = SetupIndex; break;
            }
        }
        for (int32 Offset = 1; Offset <= Count; ++Offset)
        {
            const int32 Index = (SeedCollectionBucketIndex + Offset) % Count;
            if (SeedCollectionBuckets[Index].Saved >= SeedCollectionBuckets[Index].Target
                || SeedCollectionBuckets[Index].SetupIndex != SetupToTest) {
                continue;
            }
            SeedCollectionBucketIndex = Index;
            const FSeedCollectionBucket& Bucket = SeedCollectionBuckets[Index];
            const FSeedCollectionSetup& Setup = SeedCollectionSetups[Bucket.SetupIndex];
            if (!Setup.Room.IsValid()) { bSeedCollectionEnding = true; return false; }
            CurrentRoom = Setup.Room.Get();
            CurrentRoomEntranceTag = Setup.EntranceDoorTag;
            ChosenDoorTag = Setup.DoorTag;
            SeedBankProfile = Setup.Profile;
            ActivePuzzleId = Bucket.PuzzleId;
            ActivePuzzleRoomClass = PuzzleRoomsById.FindRef(ActivePuzzleId);
            if (!SetSeedBankPuzzleFile(ActivePuzzleId)) { return false; }
            LastRouteRoomClass = Bucket.PreviousRoomClass.Get();
            ConsecutiveRouteRooms = Bucket.PreviousRoomCount;
            // Every candidate starts independently; failed seeds do not affect the next replay.
            do
            {
                CurrentLayoutSeed = NextSeedToTest;
                NextSeedToTest = NextSeedToTest == MAX_int32 ? 1 : NextSeedToTest + 1;
            } while (SeedBankEntries.ContainsByPredicate([this, &Bucket](const FSeedBankEntry& E)
                { return E.Profile == SeedBankProfile && E.Seed == CurrentLayoutSeed && E.PuzzleId == Bucket.PuzzleId
                && E.PreviousRoomClass == SeedClassPath(Bucket.PreviousRoomClass.Get()) && E.PreviousRoomCount == Bucket.PreviousRoomCount; }));
            RandomStream.Initialize(CurrentLayoutSeed);
            GenerationStartedAt = FPlatformTime::Seconds();
            NavigationStartedAt = 0.0;
            bGenerationTimedOut = false;
            bGenerationTargetLogged = false;
            ++TestedSeedCount;
            SeedCollectionStatus = FString::Printf(TEXT("Setup %d/%d, %s, puzzle %s, seed %d. Saved %d/%d; coverage %d/%d."),
                Bucket.SetupIndex + 1, SeedSetupCount, *ChosenDoorTag.ToString(), *ActivePuzzleId.ToString(), CurrentLayoutSeed,
                SavedSeedCount, TargetSavedSeedCount, CoveredSeedBuckets, RequiredSeedBuckets);
            return true;
        }
        bSeedCollectionEnding = true;
        bSeedCollectionReachedTarget = RequiredSeedBuckets > 0 && CoveredSeedBuckets == RequiredSeedBuckets
            && !SeedCollectionBuckets.ContainsByPredicate([](const FSeedCollectionBucket& Bucket) { return Bucket.Saved < Bucket.Target; });
        return false;
    }
    if (SavedSeedCandidateIndex < SavedSeedCandidates.Num())
    {
        CurrentLayoutSeed = SavedSeedCandidates[SavedSeedCandidateIndex++];
        RandomStream.Initialize(CurrentLayoutSeed);
        bSeedAttemptUsesBank = true;
        return true;
    }
    if (bSeedSessionPrepared && bUseSavedSeeds && bRequireSavedSeeds)
    {
        UE_LOG(LogTemp, Error, TEXT("Maze seeds: Approved candidates were exhausted for %s. Check changed assets, revision, bounds, and navigation."), *CurrentSeedBankFile);
        return false;
    }
    CurrentLayoutSeed = RandomStream.GetCurrentSeed();
    return true;
}

void AMazeGenerator::QueueSeedCollectionCleanup(bool bContinue)
{
    bSeedCollectionEnding = !bContinue;
    bSeedCollectionInternalCleanup = true;
    bGenerationTimedOut = false;
    bGenerationTargetLogged = false;
    GenerationStartedAt = FPlatformTime::Seconds();
    BeginGenerationCleanup(bContinue);
    bSeedCollectionInternalCleanup = false;
}

bool AMazeGenerator::HandleSeedAttemptCleanup(bool bRetry)
{
    if (!bSeedCollectionRunning || bSeedCollectionInternalCleanup || bSeedCollectionEnding
        || GenerationStage == EGenerationStage::Cleanup || GenerationStage == EGenerationStage::Idle) {
        return false;
    }
    if (!bRetry && !bGenerationTimedOut)
    {
        SeedCollectionStatus = TEXT("Stopped on a configuration error; previously saved seeds are retained.");
        QueueSeedCollectionCleanup(false);
        return true;
    }
    if (TestedSeedCount > 0 && TestedSeedCount % 100 == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Maze seeds: Tested %d candidates, saved %d/%d. Stop PIE to pause."), TestedSeedCount, SavedSeedCount, TargetSavedSeedCount);
    }
    // Seed-bank entries must pass without local rewinds or failed whole-layout attempts.
    QueueSeedCollectionCleanup(true);
    return true;
}

bool AMazeGenerator::HandleSeedAttemptFinished(bool bSuccess)
{
    if (!bSeedCollectionRunning)
    {
        if (bSuccess && bSeedAttemptUsesBank) { LastUsedBankSeeds.Add(RuntimeSeedBucketKey, CurrentLayoutSeed); }
        return false;
    }
    if (bSeedCollectionEnding || !bSuccess)
    {
        FinishSeedCollection();
        return true;
    }
    const float Seconds = static_cast<float>(FPlatformTime::Seconds() - GenerationStartedAt);
    if (MaxAcceptedSeedSeconds > 0.0f && Seconds > MaxAcceptedSeedSeconds)
    {
        SeedCollectionStatus = FString::Printf(TEXT("Seed %d passed but took %.2fs; skipped by the %.2fs collection limit."), CurrentLayoutSeed, Seconds, MaxAcceptedSeedSeconds);
        if (TestedSeedCount % 100 == 0) { UE_LOG(LogTemp, Display, TEXT("Maze seeds: %s"), *SeedCollectionStatus); }
        QueueSeedCollectionCleanup(true);
        return true;
    }
    FSeedBankEntry Entry;
    const FSeedCollectionBucket& Bucket = SeedCollectionBuckets[SeedCollectionBucketIndex];
    Entry.Profile = SeedBankProfile;
    Entry.Seed = CurrentLayoutSeed;
    Entry.PuzzleId = Bucket.PuzzleId;
    Entry.PreviousRoomClass = SeedClassPath(Bucket.PreviousRoomClass.Get());
    Entry.PreviousRoomCount = Bucket.PreviousRoomCount;
    Entry.Seconds = Seconds;
    SeedBankEntries.Add(Entry);
    if (!SaveSeedBankFile())
    {
        SeedBankEntries.RemoveAt(SeedBankEntries.Num() - 1);
        SeedCollectionStatus = TEXT("Stopped: seed file could not be saved. Check the Output Log.");
        QueueSeedCollectionCleanup(false);
        return true;
    }
    ++SeedCollectionBuckets[SeedCollectionBucketIndex].Saved;
    SavedSeedCount = SeedBankEntries.Num();
    UpdateSeedCollectionCoverage();
    bSeedCollectionReachedTarget = CoveredSeedBuckets == RequiredSeedBuckets && !SeedCollectionBuckets.ContainsByPredicate([](const FSeedCollectionBucket& B) { return B.Saved < B.Target; });
    SeedCollectionStatus = FString::Printf(TEXT("Saved %d/%d. Latest seed %d, puzzle %s, %.2fs."), SavedSeedCount, TargetSavedSeedCount, CurrentLayoutSeed, *ActivePuzzleId.ToString(), Seconds);
    UE_LOG(LogTemp, Display, TEXT("Maze seeds: %s"), *SeedCollectionStatus);
    QueueSeedCollectionCleanup(!bSeedCollectionReachedTarget);
    return true;
}

void AMazeGenerator::FinishSeedCollection()
{
    AMazeActors* Room = SeedCollectionSourceRoom;
    const FName DoorTag = SeedCollectionSourceDoorTag;
    const TArray<FName> AllowedDoors = PreviousUnlockedDoorTags;
    const bool bReached = bSeedCollectionReachedTarget && RequiredSeedBuckets > 0
        && CoveredSeedBuckets == RequiredSeedBuckets;
    if (bReached)
    {
        SeedCollectionStatus = bCollectSeedsInEditor ? TEXT("Full-loop collection complete: all four door setups and every history covered. Disable Collect Seeds In Editor to play.")
            : TEXT("Full-loop collection complete: all four door setups and every history covered. Ready for normal play.");
    }
    CancelGeneration();
    ChosenDoorTag = NAME_None;
    UnlockedDoorTags = AllowedDoors;
    LoopState = EMazeLoopState::WaitingForDoor;
    ApplyDoorLocks(CurrentRoom, UnlockedDoorTags);
    UE_LOG(LogTemp, Display, TEXT("Maze seeds: %s Saved %d/%d. File: %s"), *SeedCollectionStatus, SavedSeedCount, TargetSavedSeedCount, *CurrentSeedBankFile);
    // End the door's pending interaction without opening onto a cleared test layout.
    OnMazeGenerationFinished.Broadcast(Room, DoorTag, false);
    OnSeedCollectionFinished.Broadcast(bReached);
}

void AMazeGenerator::ResetSeedSession()
{
    if (SeedCollectionSourceRoom)
    {
        CurrentRoom = SeedCollectionSourceRoom;
        CurrentRoomEntranceTag = SeedCollectionSourceEntranceTag;
        ChosenDoorTag = SeedCollectionSourceDoorTag;
        LastRouteRoomClass = SeedCollectionOriginalRoomClass;
        ConsecutiveRouteRooms = SeedCollectionOriginalRoomCount;
        ActivePuzzleId = SeedCollectionOriginalPuzzleId;
        ActivePuzzleRoomClass = SeedCollectionOriginalPuzzleClass;
        RandomStream = SeedCollectionOriginalRandomStream;
        if (IsValid(MazeNavBounds))
        {
            MazeNavBounds->SetActorTransform(SeedCollectionOriginalNavTransform);
            if (UBrushComponent* Brush = MazeNavBounds->GetBrushComponent()) { Brush->UpdateBounds(); }
            if (UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld())) { Navigation->OnNavigationBoundsUpdated(MazeNavBounds.Get()); }
        }
    }
    if (IsValid(SeedCollectionPreviewRoom)) { DiscardMazePiece(SeedCollectionPreviewRoom); }
    SeedCollectionSourceRoom = nullptr;
    SeedCollectionPreviewRoom = nullptr;
    SeedCollectionSourceDoorTag = NAME_None;
    SeedCollectionSourceEntranceTag = NAME_None;
    SeedCollectionOriginalPuzzleId = NAME_None;
    SeedCollectionOriginalPuzzleClass = nullptr;
    SeedCollectionSetups.Reset();
    bSeedCollectionRunning = false;
    bSeedCollectionEnding = false;
    bSeedCollectionInternalCleanup = false;
    bSeedCollectionReachedTarget = false;
    bSeedAttemptUsesBank = false;
    bSeedSessionPrepared = false;
    SeedCollectionOriginalRoomClass = nullptr;
    SeedCollectionOriginalRoomCount = 0;
    SeedCollectionBuckets.Reset();
    SeedBankEntries.Reset();
    SavedSeedCandidates.Reset();
    SavedSeedCandidateIndex = 0;
    RuntimeSeedBucketKey.Empty();
}