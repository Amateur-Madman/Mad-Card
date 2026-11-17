// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/MadManController.h"
#include "Components/SplineComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavMesh/NavMeshPath.h"
#include "Actors/MadMan_Agent.h"

void AMadManController::FollowSpline(USplineComponent* SplineToFollow, bool bShouldLoop)
{
	if (!SplineToFollow || !GetPawn() || !GetPathFollowingComponent())
	{
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return;
	}

	TargetSpline = SplineToFollow;
	bLoopSpline = bShouldLoop;

	const float SplineLength = TargetSpline->GetSplineLength();
	const float SampleSpacing = 100.0f; // Captures curves accurately without manual tuning

	// 1. Create a new Navigation Path instance
	FNavPathSharedPtr CustomNavPath = MakeShareable(new FNavigationPath());
	TArray<FNavPathPoint>& PathPoints = CustomNavPath->GetPathPoints();

	// Start with the AI's current location on the NavMesh
	FNavLocation StartNavLoc;
	if (NavSys->ProjectPointToNavigation(GetPawn()->GetActorLocation(), StartNavLoc))
	{
		PathPoints.Add(FNavPathPoint(StartNavLoc.Location));
	}

	// 2. Sample points along the entire spline curve and project them to the NavMesh
	for (float Dist = 0.0f; Dist <= SplineLength; Dist += SampleSpacing)
	{
		const FVector RawPoint = TargetSpline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		FNavLocation ProjectedLoc;

		if (NavSys->ProjectPointToNavigation(RawPoint, ProjectedLoc, FVector(100.0f, 100.0f, 250.0f)))
		{
			// Avoid adding duplicate points if two samples snap to the exact same spot
			if (PathPoints.Num() == 0 || !PathPoints.Last().Location.Equals(ProjectedLoc.Location, 5.0f))
			{
				PathPoints.Add(FNavPathPoint(ProjectedLoc.Location));
			}
		}
	}

	// Ensure the exact end point of the spline is included
	const FVector EndRawPoint = TargetSpline->GetLocationAtDistanceAlongSpline(SplineLength, ESplineCoordinateSpace::World);
	FNavLocation EndProjectedLoc;
	if (NavSys->ProjectPointToNavigation(EndRawPoint, EndProjectedLoc, FVector(100.0f, 100.0f, 250.0f)))
	{
		PathPoints.Add(FNavPathPoint(EndProjectedLoc.Location));
	}

	// Need at least 2 points (start and end) to form a valid path
	if (PathPoints.Num() < 2)
	{
		return;
	}

	// Mark the path as ready so PathFollowingComponent accepts it
	CustomNavPath->MarkReady();

	// 3. Hand the entire curved path to the PathFollowingComponent in one call
	FAIMoveRequest MoveReq(PathPoints.Last().Location);
	MoveReq.SetAcceptanceRadius(25.0f);
	MoveReq.SetUsePathfinding(false); // We already built the path!

	GetPathFollowingComponent()->RequestMove(MoveReq, CustomNavPath);
}

void AMadManController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if(Result.IsSuccess())
	{
		if (bLoopSpline)
		{
			FollowSpline(TargetSpline, bLoopSpline);
		}
		else
		{
			TargetSpline = nullptr; // Clear the spline reference if not looping
		}

		if (AMadMan_Agent* Agent = Cast<AMadMan_Agent>(GetPawn()))
		{
			Agent->OnIntroSpot.Broadcast();
		}
	}
}