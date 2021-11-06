// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMoveTool, Display, All);
DECLARE_LOG_CATEGORY_EXTERN(LogRotateTool, Display, All);
DECLARE_LOG_CATEGORY_EXTERN(LogScaleTool, Display, All);

enum EToolAxisLock
{
	X,
	Y,
	Z,
	NONE
};

struct FAxisLockHelper
{
	bool IsDualAxisLock = false;
	EToolAxisLock CurrentLockedAxis = NONE;
	bool IsWorldSpace = false;
	FTransform TransformWhenLocked = FTransform::Identity;
	FVector LockVector = FVector::ZeroVector;
	FVector LockPlaneNormal = FVector::ZeroVector;
	bool isLocked() { return CurrentLockedAxis != NONE ? true : false; };
};

struct FGroupTransform
{
	struct FChildTransform
	{
		FChildTransform(AActor* InActor, const FIntPoint& InScreenSpaceOffset)
			: Actor(InActor), ChildOriginalTransform(InActor->GetTransform()), ScreenSpaceOffset(InScreenSpaceOffset) {};

		AActor* Actor;
		FTransform ChildOriginalTransform;
		FVector RelativeOffset;
		FIntPoint ScreenSpaceOffset;
	};

	void SetAverageLocation();

	FVector GetOriginLocation() { return Parent.GetLocation(); }
	FTransform GetParentTransform() { return Parent; };

	FVector GetLocalForwardVector() { return Parent.GetRotation().GetForwardVector(); };
	FVector GetLocalRightVector() { return Parent.GetRotation().GetRightVector(); };
	FVector GetLocalUpVector() { return Parent.GetRotation().GetUpVector(); };

	void SetTransform(FTransform InTransform) {};
	void AddRotation(const FRotator& InAddRotation);
	void SetLocation(const FVector& InNewLocation);
	void SetScale(const FVector& InNewScale, const FVector& ScaleAxis, bool bUniformScale);
	void AddChild(AActor* NewChild, const FIntPoint& InScreenspaceOffset);
	void FinishSetup(class FEditorViewportClient* InViewportClient);

public:
	int32 GetNumChildren() { return Children.Num(); };
	FIntPoint GetScreenSpaceOffset() { return ScreenSpaceParentCursorOffset; };

private:
	FTransform Parent;
	FTransform ParentOriginalTransform;
	FIntPoint ScreenSpaceParentCursorOffset;
	TArray<FChildTransform> Children;
	const UWorld* CurrentWorld;
};

class FBlenderToolMode
{
public:
	FBlenderToolMode(class FEditorViewportClient* InViewportClient, FText InOperationName)
		: ToolViewportClient(InViewportClient), OperationName(InOperationName)
	{
		ToolBegin();
	}

	/** 
	* Canceling any transaction in the destructor ensures that we don't enter some undefined transaction state.
	* Regular transaction closing is handled by ToolClose() 
	*/
	virtual ~FBlenderToolMode() { GEditor->CancelTransaction(0); };

	// Main tool functions
	virtual void ToolBegin();
	virtual void ToolUpdate() {};
	virtual void ToolClose(bool Success);

	struct FSelectionToolHelper
	{
		FSelectionToolHelper(AActor* InActor, FTransform InTransform)
			: Actor(InActor), DefaultTransform(InTransform) {};

		AActor* Actor;
		FTransform DefaultTransform;
	};

	TArray<AActor*> GetSelectedActors() 
	{
		TArray<AActor*> Actors;
		for (auto& Info : SelectionInfos) { Actors.Add(Info.Actor); }
		return Actors;
	}

	FVector GetCameraForwardVector() { return ToolViewportClient->GetViewRotation().Vector(); };
	TSharedPtr<FGroupTransform> GetGroupTransform() { return GroupTransform; };

	virtual void SetAxisLock(const EToolAxisLock& InAxisToLock, bool bDualAxis);
	bool IsSingleSelection() { return SelectionInfos.Num() == 1 ? true : false; }

protected:

	void CalculateAxisLock();

	/** Draws the lines in the viewport that are visible when an axis lock is active */
	virtual void DrawAxisLocks();

	class FEditorViewportClient* ToolViewportClient;
	TSharedPtr<FGroupTransform> GroupTransform;
	TArray<FSelectionToolHelper> SelectionInfos;
	FAxisLockHelper AxisLockHelper;
private:
	const FText OperationName;
	FLinearColor DefaultSelectionOutlineColor;
	TArray<FAxisLineDrawHelper> AxisLineDrawHelper;
};

class FMoveMode : public FBlenderToolMode
{
public:

	FMoveMode(FEditorViewportClient* InViewportClient, FText InOperationName)
		:FBlenderToolMode(InViewportClient, InOperationName) {
		ToolBegin();
	}

	virtual void ToolBegin() override;
	virtual void ToolUpdate() override;
	virtual void ToolClose(bool Success) override;

private:

	FIntPoint ScreenSpaceOriginOffset;
};

class FRotateMode : public FBlenderToolMode
{
public:

	FRotateMode(FEditorViewportClient* InViewportClient, FText InOperationName)
		:FBlenderToolMode(InViewportClient, InOperationName) {
		ToolBegin();
	}

	virtual void ToolBegin() override;
	virtual void ToolUpdate() override;
	virtual void ToolClose(bool Success) override;

	virtual void SetAxisLock(const EToolAxisLock& InAxisToLock, bool bDualAxis) override;

private:

	FVector LastUpdateMouseRotVector;
	FIntPoint LastCursorLocation;
};

class FScaleMode : public FBlenderToolMode
{
public:

	FScaleMode(FEditorViewportClient* InViewportClient, FText InOperationName)
		:FBlenderToolMode(InViewportClient, InOperationName) {
		ToolBegin();
	}

	virtual void ToolBegin() override;
	virtual void ToolUpdate() override;
	virtual void ToolClose(bool Success) override;

private:
	float StartDistance;
	FIntPoint ActorScreenPosition;
};