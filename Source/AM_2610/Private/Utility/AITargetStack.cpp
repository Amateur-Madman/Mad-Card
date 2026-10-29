// Fill out your copyright notice in the Description page of Project Settings.


#include "Utility/AITargetStack.h"

void UAITargetStack::Push(AActor* Target)
{
	if (!IsValid(Target))
	{
		return;
	}

	Remove(Target);
	Stack.Add(Target);
}

void UAITargetStack::Remove(AActor* Target)
{
	Stack.RemoveAll([Target](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Target;
	});
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
