// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class FBlenderViewportControlsEdMode : public FEdMode
{
public:
	const static FEditorModeID EM_BlenderViewportControlsEdModeId;

	// FEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool UsesTransformWidget() const override { return false; }
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	bool UsesToolkits() const override { return false; }
	// End of FEdMode interface

	class ATransformGroupActor* GetTransformGroupActor() { return TransformGroupActor; }
	
protected:

	/** Transform Reset Function */
	void ResetSpecificActorTransform(void(*DoReset)(AActor*));

	bool IsOperationInProgress() { return ActiveToolMode ? true : false; };

	void FinishActiveOperation(bool InAccepted = true);

	bool HasActiveSelection();

	void CreateTransformActor();

	/** Delegate handle for registered selection change lambda */
	FDelegateHandle SelectionChangedHandle;

private:
	
	/** The active tool, e.g Moving an Object, Rotating an Object, Scaling an Object */
	TSharedPtr<class FBlenderToolMode> ActiveToolMode;

	/** A helper actor to make transform operations easier */
	class ATransformGroupActor* TransformGroupActor;
};
