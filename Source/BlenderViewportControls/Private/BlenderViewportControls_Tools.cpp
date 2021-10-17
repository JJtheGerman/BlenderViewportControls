// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_Tools.h"
#include "BlenderViewportControls_HelperFunctions.h"
#include "ViewportWorldInteraction.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Selection.h"

DEFINE_LOG_CATEGORY(LogMoveTool);
DEFINE_LOG_CATEGORY(LogRotateTool);
DEFINE_LOG_CATEGORY(LogScaleTool);

/**
 * Base Implementation of the FBlenderToolMode
 */
void FBlenderToolMode::ToolBegin()
{
	// Change the selection outline color when in ToolMode
	GEditor->SetSelectionOutlineColor(FLinearColor::White);

	// Save our selection and their transforms so we can reset in case we cancel the operation.
	SaveSelectedActorsTransforms(GEditor->GetSelectedActors());
}

void FBlenderToolMode::ToolClose(bool Success /*= true*/)
{
	// TODO: Save the default SelectionOutlineColor on EditorMode start and make it publicly accessible. This might break if users messed with their default outline colors.
	GEditor->SetSelectionOutlineColor(GEditor->GetSelectedMaterialColor());

	if (!Success)
	{
		for (auto& Defaults : DefaultTransforms)
		{
			Defaults.Actor->SetActorTransform(Defaults.DefaultTransform);
		}

		GEditor->CancelTransaction(0);
		return;
	}

	// End the transaction so we can undo it later.
	GEditor->EndTransaction();
}

void FBlenderToolMode::SaveSelectedActorsTransforms(class USelection* Selection)
{
	for (FSelectionIterator Iter(*Selection); Iter; ++Iter)
	{
		if (AActor* LevelActor = Cast<AActor>(*Iter))
		{
			DefaultTransforms.Add(FSelectionToolHelper(LevelActor, LevelActor->GetTransform()));
		}
	}
}



/**
 * Move Tool Implementation
 */
void FMoveMode::ToolBegin()
{
	UE_LOG(LogMoveTool, Verbose, TEXT("Begin"));

	// Begins the Transaction so we can undo it later
	GEditor->BeginTransaction(FText::FromString("BlenderMode: MoveActor"));

	// Project the cursor from the screen to the world
	TTuple<FVector, FVector> WorldLocDir = TransformHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector Intersection = TransformHelperFunctions::LinePlaneIntersectionCameraObject(ToolViewportClient, CursorWorldPosition, CursorWorldDirection);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		if (AActor* LevelActor = Cast<AActor>(*Iter))
		{
			FMoveToolSelectionHelper MoveHelper(LevelActor, LevelActor->GetTransform());
			// We store an offset for each actor from the cursor so they stay at their relative location when moving
			MoveHelper.SelectionOffset = Intersection - LevelActor->GetActorLocation();
			Selections.Add(MoveHelper);
		}
	}
}

void FMoveMode::ToolUpdate()
{
	TTuple<FVector, FVector> WorldLocDir = TransformHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector CursorIntersection = TransformHelperFunctions::LinePlaneIntersectionCameraObject(ToolViewportClient, CursorWorldPosition, CursorWorldDirection);

	for (auto& Selection : Selections)
	{
		FVector NewLocation = CursorIntersection - Selection.SelectionOffset;
		Selection.Actor->Modify();
		Selection.Actor->SetActorLocation(NewLocation);
	}

	if (ToolViewportClient->IsCtrlPressed() && Selections.Num() == 1)
	{
		UWorld* World = ToolViewportClient->GetWorld();
		if (World)
		{
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(Selections[0].Actor);
			if (World->LineTraceSingleByChannel(HitResult, CursorWorldPosition, CursorWorldPosition + (CursorWorldDirection * 100000.f), ECC_Visibility, QueryParams))
			{
				Selections[0].Actor->SetActorLocation(HitResult.ImpactPoint);
				Selections[0].Actor->SetActorRotation(HitResult.ImpactNormal.Rotation());
			}
		}
	}
}

void FMoveMode::ToolClose(bool Success)
{
	FBlenderToolMode::ToolClose(Success);

	UE_LOG(LogMoveTool, Verbose, TEXT("Closed"));
}


/**
 * Rotate Tool Implementation
 */
void FRotateMode::ToolBegin()
{
	UE_LOG(LogRotateTool, Verbose, TEXT("Begin"));

	// Begins the Transaction so we can undo it later
	GEditor->BeginTransaction(FText::FromString("BlenderMode: RotateActor"));

	// Project the cursor from the screen to the world
	TTuple<FVector, FVector> WorldLocDir = TransformHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector Intersection = TransformHelperFunctions::LinePlaneIntersectionCameraObject(ToolViewportClient, CursorWorldPosition, CursorWorldDirection);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		if (AActor* LevelActor = Cast<AActor>(*Iter))
		{
			FSelectionToolHelper ToolHelper(LevelActor, LevelActor->GetTransform());
			// We store an offset for each actor from the cursor so they stay at their relative location when moving
			DefaultTransforms.Add(ToolHelper);
		}
	}

	// TODO: Change this code to check if there are multiple selections and then use the bounding box center instead.
	LastUpdateMouseRotVector = (Intersection - DefaultTransforms[0].Actor->GetActorLocation()).GetSafeNormal();
}

void FRotateMode::ToolUpdate()
{
	TTuple<FVector, FVector> WorldLocDir = TransformHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector CursorIntersection = TransformHelperFunctions::LinePlaneIntersectionCameraObject(ToolViewportClient, CursorWorldPosition, CursorWorldDirection);

	FVector currentRotVector = (CursorIntersection - DefaultTransforms[0].Actor->GetActorLocation()).GetSafeNormal();

	// Gives us the degrees between the cursor starting position and end position in -180 - 180;
	float degreesBetweenVectors = UKismetMathLibrary::DegAcos(FVector::DotProduct(currentRotVector, LastUpdateMouseRotVector));

	// First if check if the camera is looking down or up and flips. Otherwise dragging the mouse left would rotate the object
	// to the right whenever we are looking from below. (Hard to explain, just uncomment the first "if" statement to see the behavior.
	if (ToolViewportClient->GetViewRotation().Vector().Z > 0)
	{
		degreesBetweenVectors = degreesBetweenVectors * -1.f;
	}
	if (FVector::CrossProduct(currentRotVector, LastUpdateMouseRotVector).Z < 0.f)
	{
		degreesBetweenVectors = -degreesBetweenVectors;
	}

	FRotator AddRotation = UKismetMathLibrary::RotatorFromAxisAndAngle(ToolViewportClient->GetViewRotation().Vector(), degreesBetweenVectors);

	if (LastCursorLocation == ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos())
	{
		AddRotation = FRotator::ZeroRotator;
	}

	DefaultTransforms[0].Actor->AddActorWorldRotation(AddRotation);

	LastUpdateMouseRotVector = (CursorIntersection - DefaultTransforms[0].Actor->GetActorLocation()).GetSafeNormal();
	LastCursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();
}

void FRotateMode::ToolClose(bool Success)
{
	FBlenderToolMode::ToolClose(Success);

	UE_LOG(LogRotateTool, Verbose, TEXT("Closed"));
}



/**
 * Scale Tool Implementation
 */
void FScaleMode::ToolBegin()
{
	UE_LOG(LogScaleTool, Verbose, TEXT("Begin"));

	// Begins the Transaction so we can undo it later
	GEditor->BeginTransaction(FText::FromString("BlenderMode: ScaleActor"));
	
	FIntPoint CursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	USelection* Selection = GEditor->GetSelectedActors();
	if (AActor* SelectedActor = Cast<AActor>(Selection->GetSelectedObject(0)))
	{
		ActorScreenPosition = TransformHelperFunctions::ProjectWorldLocationToScreen(ToolViewportClient, SelectedActor->GetActorLocation());
		StartDistance = FVector2D::Distance((FVector2D)ActorScreenPosition, (FVector2D)CursorLocation);
	}
}

void FScaleMode::ToolUpdate()
{
	FIntPoint CursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	float CurrentDistance;
	USelection* Selection = GEditor->GetSelectedActors();
	if (AActor* SelectedActor = Cast<AActor>(Selection->GetSelectedObject(0)))
	{
		CurrentDistance = FVector2D::Distance((FVector2D)ActorScreenPosition, (FVector2D)CursorLocation);
		float NewScaleMultiplier = CurrentDistance / StartDistance;

		SelectedActor->Modify();
		SelectedActor->SetActorScale3D(DefaultTransforms[0].DefaultTransform.GetScale3D() * NewScaleMultiplier);
	}
}

void FScaleMode::ToolClose(bool Success)
{
	FBlenderToolMode::ToolClose(Success);
	UE_LOG(LogScaleTool, Verbose, TEXT("Closed"));
}
