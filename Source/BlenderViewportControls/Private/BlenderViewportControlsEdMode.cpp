// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControlsEdMode.h"
#include "BlenderViewportControls_Tools.h"
#include "BlenderViewportControls_GroupActor.h"
#include "BlenderViewportControls_HelperFunctions.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"
#include "Editor/EditorEngine.h"
#include "EditorWorldExtension.h"
#include "Kismet/KismetMathLibrary.h"
#include "ViewportWorldInteraction.h"
#include "EngineUtils.h"
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
		if (ActiveToolMode) { ActiveToolMode->ToolClose(false); }
		ActiveToolMode = nullptr;
	});

	// An empty actor that exists so we can simplify multi object transforms. Instead of doing a whole lot of math
	// we simply parent selected objects directly to it and modify the transform actor instead
	CreateTransformActor();
}

/** FEdMode: Called when user Exits the Mode */
void FBlenderViewportControlsEdMode::Exit()
{
	// Unbind delegates
	USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);

	// Destroy the group actor when we exit the editor mode
	TransformGroupActor->Destroy();

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
			ActiveToolMode = MakeShared<FRotateMode>(InViewportClient, FText::FromString(TEXT("BlenderTool: Rotate")));
			return true;
		}

		// Enter Actor Scale Mode
		if (InKey == EKeys::S && InEvent != IE_Released)
		{
			ActiveToolMode = MakeShared<FScaleMode>(InViewportClient, FText::FromString(TEXT("BlenderTool: Scale")));
			return true;
		}
	}

	/**  Axis Constraints **/
	// Only check these binds when we are in an active tool mode so we don't consume default editor input.
	// E.g Ctrl + Z would not work without this.
	if (IsOperationInProgress())
	{
		// Constrain to X axis
		if (InKey == EKeys::X && InEvent != IE_Released)
		{
			return true;
		}

		// Constrain to Y axis
		if (InKey == EKeys::Y && InEvent != IE_Released)
		{
			return true;
		}

		// Constrain to Z axis
		if (InKey == EKeys::Z && InEvent != IE_Released)
		{
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
	GEditor->BeginTransaction(FText::FromString("BlenderTool: ResetTransform"));
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

bool FBlenderViewportControlsEdMode::HasActiveSelection()
{
	return GEditor->GetSelectedActorCount() > 0 ? true : false;
}

void FBlenderViewportControlsEdMode::CreateTransformActor()
{
	UWorld* World = GetWorld();
	const bool bWasWorldPackageDirty = World->GetOutermost()->IsDirty();

	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActorSpawnParameters.ObjectFlags = EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient;
	ActorSpawnParameters.bHideFromSceneOutliner = true;

	AActor* NewActor = World->SpawnActor<AActor>(ATransformGroupActor::StaticClass(), ActorSpawnParameters);

	// Give the new actor a root scene component, so we can attach multiple sibling components to it
	USceneComponent* SceneComponent = NewObject<USceneComponent>(NewActor);
	SceneComponent->SetMobility(EComponentMobility::Static);
	NewActor->AddOwnedComponent(SceneComponent);
	NewActor->SetRootComponent(SceneComponent);
	SceneComponent->RegisterComponent();

	// Don't dirty the level file after spawning
	if (!bWasWorldPackageDirty)
	{
		World->GetOutermost()->SetDirtyFlag(false);
	}

	TransformGroupActor = CastChecked<ATransformGroupActor>(NewActor);
}
