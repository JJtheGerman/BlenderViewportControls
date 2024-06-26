// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControlsEdMode.h"
#include "BlenderViewportControls_Tools.h"
#include "BlenderViewportControls_HelperFunctions.h"
#include "Editor/EditorEngine.h"
#include "DrawDebugHelpers.h"
#include "Engine/Selection.h"

const FEditorModeID FBlenderViewportControlsEdMode::EM_BlenderViewportControlsEdModeId = TEXT("EM_BlenderViewportControlsEdMode");

extern UNREALED_API UEditorEngine* GEditor;

/** FEdMode: Called when user Enters the Mode */
void FBlenderViewportControlsEdMode::Enter()
{
	FEdMode::Enter();
	
	// If we lose our selection, set the SharedPtr to null to get rid of the active tool
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddLambda([&](UObject* Object)
	{
		if(ActiveToolMode)
		{
			ActiveToolMode->ToolClose(false);
		}
		
		ActiveToolMode = nullptr;
	});
}

/** FEdMode: Called when user Exits the Mode */
void FBlenderViewportControlsEdMode::Exit()
{
	// Unbind delegates
	USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

/** FEdMode: Called every frame as long as the Mode is active */
void FBlenderViewportControlsEdMode::Tick(FEditorViewportClient* InViewportClient, float DeltaTime)
{
	if (ActiveToolMode)
	{
		// Update the active tool
		ActiveToolMode->ToolUpdate();
	}
}

void FBlenderViewportControlsEdMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	if (ActiveToolMode)
	{
		// Let Tools draw their own viewport HUD visualizations
		ActiveToolMode->DrawHUD(ViewportClient, Viewport, View, Canvas);
	}
}

/** FEdMode: Called when a key is pressed */
bool FBlenderViewportControlsEdMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	// Modifier key states
	const bool bAltDown = InViewportClient->IsAltPressed();
	const bool bShiftDown = InViewportClient->IsShiftPressed();
	const bool bControlDown = InViewportClient->IsCtrlPressed();

	// Accept Operation
	if (InKey == EKeys::LeftMouseButton && InEvent != IE_Released)
	{
		if (IsOperationInProgress())
		{
			FinishActiveOperation(true);
			return true;
		}
	}

	// Cancel Operation
	if (InKey == EKeys::RightMouseButton && InEvent != IE_Released)
	{
		if (IsOperationInProgress())
		{
			FinishActiveOperation(false);
			return true;
		}
	}

	/** Transform Modes **/
	// If alt is down G,R,S are instead resetting transforms
	if (!bAltDown && !InViewportClient->IsFlightCameraActive() && HasActiveSelection())
	{
		// Enter Actor Move Mode
		if (InKey == EKeys::G && InEvent != IE_Released)
		{
			ActiveToolMode = MakeShared<FMoveMode>(InViewportClient, FText::FromString(TEXT("BlenderTool: Move")));
			return true;
		}

		// Enter Actor Rotate Mode
		if (InKey == EKeys::R && InEvent != IE_Released)
		{
			// The Rotate tool has a special trackball rotation mode that can be entered by pressing R again while the RotateMode is active
			if (IsOperationInProgress() && IsRotateMode())
			{
				const TSharedPtr<FRotateMode> RotateMode = StaticCastSharedPtr<FRotateMode>(ActiveToolMode);
				RotateMode->ToggleTrackBallRotation();
			}
			else
			{
				ActiveToolMode = MakeShared<FRotateMode>(InViewportClient, FText::FromString(TEXT("BlenderTool: Rotate")));
			}
			
			return true;
		}

		// Enter Actor Scale Mode
		if (InKey == EKeys::S && InEvent != IE_Released)
		{
			ActiveToolMode = MakeShared<FScaleMode>(InViewportClient, FText::FromString(TEXT("BlenderTool: Scale")));
			return true;
		}
	}

	/** Snap Offset Increments **/
	if (IsOperationInProgress() && bControlDown)
	{
		if (InKey == EKeys::MouseScrollUp)
		{
			ActiveToolMode->AddSnapOffset(1.f);
			return true;
		}

		if (InKey == EKeys::MouseScrollDown)
		{
			ActiveToolMode->AddSnapOffset(-1.f);
			return true;
		}
	}

	/**  Axis Constraints **/
	// Only check these binds when we are in an active tool mode so we don't consume default editor input.
	// E.g Ctrl + Z would not work without this.
	if (IsOperationInProgress())
	{
		const bool IsDualAxisLock = bShiftDown;

		// Constrain to X axis
		if (InKey == EKeys::X && InEvent != IE_Released)
		{
			ActiveToolMode->SetAxisLock(X, IsDualAxisLock);
			return true;
		}

		// Constrain to Y axis
		if (InKey == EKeys::Y && InEvent != IE_Released)
		{
			ActiveToolMode->SetAxisLock(Y, IsDualAxisLock);
			return true;
		}

		// Constrain to Z axis
		if (InKey == EKeys::Z && InEvent != IE_Released)
		{
			ActiveToolMode->SetAxisLock(Z, IsDualAxisLock);
			return true;
		}
	}

	/** Transform Resets **/
	// Reset is only available when we aren't currently in another mode
	if (!IsOperationInProgress())
	{
		// Reset Actor Location
		if (InKey == EKeys::G && InEvent != IE_Released && bAltDown)
		{
			ResetSpecificActorTransform([](AActor* Actor) { Actor->SetActorLocation(FVector::ZeroVector); });
			return true;
		}

		// Reset Actor Rotation
		if (InKey == EKeys::R && InEvent != IE_Released && bAltDown)
		{
			ResetSpecificActorTransform([](AActor* Actor) { Actor->SetActorRotation(FRotator::ZeroRotator); });
			return true;
		}

		// Reset Actor Scale
		if (InKey == EKeys::S && InEvent != IE_Released && bAltDown)
		{
			ResetSpecificActorTransform([](AActor* Actor) { Actor->SetActorScale3D(FVector::OneVector); });
			return true;
		}
	}

	/** Duplicate Selection */
	if (!IsOperationInProgress())
	{
		if (InKey == EKeys::D && InEvent != IE_Released && bShiftDown)
		{
			DuplicateSelection(InViewportClient);
			return true;
		}
	}

	return false;
}

void FBlenderViewportControlsEdMode::ResetSpecificActorTransform(void(*DoReset)(AActor*))
{
	// The selection transform resets should only work when we are not in an active operation and we have something selected
	if (IsOperationInProgress() || !HasActiveSelection())
	{
		return;
	}

	// Start Transaction
	GEditor->BeginTransaction(FText::FromString(TEXT("BlenderTool: ResetTransform")));
	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		if (AActor* LevelActor = Cast<AActor>(*Iter))
		{
			LevelActor->Modify();
			DoReset(LevelActor);
		}
	}

	// End Transaction
	GEditor->EndTransaction();
}

void FBlenderViewportControlsEdMode::FinishActiveOperation(bool Success /** False = CancelOperation **/)
{
	if (Success)
	{
		ActiveToolMode->ToolClose(true);
	}
	else
	{
		ActiveToolMode->ToolClose(false);
	}

	ActiveToolMode = nullptr;
}

bool FBlenderViewportControlsEdMode::IsRotateMode() const
{
	if (TSharedPtr<FRotateMode> Mode = StaticCastSharedPtr<FRotateMode>(ActiveToolMode))
	{
		return true;
	}

	return false;
}

bool FBlenderViewportControlsEdMode::HasActiveSelection()
{
	return GEditor->GetSelectedActorCount() > 0;
}

void FBlenderViewportControlsEdMode::DuplicateSelection(FEditorViewportClient* InViewportClient)
{
	GEditor->BeginTransaction(FText::FromString(TEXT("BlenderTool: Duplicate")));
	GEditor->Exec(GetWorld(), *FString("DUPLICATE"));
	GEditor->EndTransaction();
	
	// We want to activate the move tool right away after duplication so the user can easily move the duplicates.
	ActiveToolMode = MakeShared<FMoveMode>(InViewportClient, FText::FromString(TEXT("BlenderTool: Move")));
}
