// Fill out your copyright notice in the Description page of Project Settings.


#include "Utility/AITargetStack.h"

void UAITargetStack::Push(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	AActor* OldTop = GetCurrentTarget();

	RemoveInternal(Target);
	Stack.Add(Target);

	if (OldTop != Target)
	{
		OnTargetChanged.Broadcast(Target);
	}
}

void UAITargetStack::Remove(AActor* Target)
{
	AActor* OldTop = GetCurrentTarget();

	RemoveInternal(Target);

	AActor* NewTop = GetCurrentTarget();
	if (OldTop != NewTop)
	{
		OnTargetChanged.Broadcast(NewTop);
	}
}

AActor* UAITargetStack::GetCurrentTarget()
{
	while (Stack.Num() > 0)
	{
		AActor* Top = Stack.Last().Get();
		if (IsValid(Top))
		{
			return Top;
		}
		Stack.Pop();
	}
	return nullptr;
}

void UAITargetStack::RemoveInternal(AActor* Target)
{
	Stack.RemoveAll([Target](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Target;
	});
}
