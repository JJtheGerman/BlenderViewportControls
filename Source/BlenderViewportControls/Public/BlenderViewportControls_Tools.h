// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMoveTool, Display, All);
DECLARE_LOG_CATEGORY_EXTERN(LogRotateTool, Display, All);
DECLARE_LOG_CATEGORY_EXTERN(LogScaleTool, Display, All);

class FBlenderToolMode
{
public:
	FBlenderToolMode(class FEditorViewportClient* InViewportClient)
		: ToolViewportClient(InViewportClient)
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

protected:
	class FEditorViewportClient* ToolViewportClient;
	TArray<FSelectionToolHelper> SelectionInfos;
	ATransformGroupActor* TransformGroupActor;
};

class FMoveMode : public FBlenderToolMode
{
public:

	FMoveMode(FEditorViewportClient* InViewportClient)
		:FBlenderToolMode(InViewportClient) {
		ToolBegin();
	}

	virtual void ToolBegin() override;
	virtual void ToolUpdate() override;
	virtual void ToolClose(bool Success) override;

private:

	FVector SelectionOffset;
};

class FRotateMode : public FBlenderToolMode
{
public:

	FRotateMode(FEditorViewportClient* InViewportClient)
		:FBlenderToolMode(InViewportClient) {
		ToolBegin();
	}

	virtual void ToolBegin() override;
	virtual void ToolUpdate() override;
	virtual void ToolClose(bool Success) override;

private:

	FVector LastUpdateMouseRotVector;
	FIntPoint LastCursorLocation;
};

class FScaleMode : public FBlenderToolMode
{
public:

	FScaleMode(FEditorViewportClient* InViewportClient)
		:FBlenderToolMode(InViewportClient) {
		ToolBegin();
	}

	virtual void ToolBegin() override;
	virtual void ToolUpdate() override;
	virtual void ToolClose(bool Success) override;

private:
	float StartDistance;
	FIntPoint ActorScreenPosition;
};