// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_Tools.h"
#include "BlenderViewportControls_HelperFunctions.h"
#include "BlenderViewportControls_GroupActor.h"
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
	DefaultSelectionOutlineColor = GEditor->GetSelectionOutlineColor();
	GEditor->SetSelectionOutlineColor(FLinearColor::White);	
	

	// Getting a reference to the group actor that is created when the editor mode is activated.
	TransformGroupActor = ToolHelperFunctions::GetTransformGroupActor();

	USelection* CurrentSelection = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*CurrentSelection); Iter; ++Iter)
	{
		if (AActor* LevelActor = Cast<AActor>(*Iter))
		{
			SelectionInfos.Add(FSelectionToolHelper(LevelActor, LevelActor->GetTransform()));
		}
	}

	// Setting the transform origin to the average location of selection.
	TransformGroupActor->SetActorLocation(ToolHelperFunctions::GetAverageLocation(GetSelectedActors()));
	// Zeroing Scale and Rotation to make Actor attach/detach easier.
	TransformGroupActor->SetActorScale3D(FVector::OneVector);
	TransformGroupActor->SetActorRotation(FRotator::ZeroRotator);

	// Start Parent Transaction
	GEditor->BeginTransaction(OperationName);

	for (auto& Info : SelectionInfos)
	{
		// Mark modified for undo system
		Info.Actor->Modify();
		TransformGroupActor->Modify();

		FAttachmentTransformRules AttachRules(FAttachmentTransformRules::KeepWorldTransform);
		Info.Actor->AttachToActor(TransformGroupActor, AttachRules);
	}
}

void FBlenderToolMode::ToolClose(bool Success /*= true*/)
{
	// Reset the selection outline color
	GEditor->SetSelectionOutlineColor(DefaultSelectionOutlineColor);

	for (auto& Info : SelectionInfos)
	{
		Info.Actor->Modify();

		FDetachmentTransformRules DetachmentRules(FDetachmentTransformRules::KeepWorldTransform);
		DetachmentRules.ScaleRule = EDetachmentRule::KeepWorld;
		Info.Actor->DetachFromActor(DetachmentRules);
	}
	
	// Ends the child transaction
	GEditor->EndTransaction();

	if (!Success)
	{
		for (auto& Info : SelectionInfos)
		{
			Info.Actor->SetActorTransform(Info.DefaultTransform);
		}

		GEditor->CancelTransaction(0);
	}

	// End the parent transaction so we can undo it.
	GEditor->EndTransaction();
}



/**
 * Move Tool Implementation
 */
void FMoveMode::ToolBegin()
{
	UE_LOG(LogMoveTool, Verbose, TEXT("Begin"));

	// Begins the child transaction
	GEditor->BeginTransaction(FText());

	// Project the cursor from the screen to the world
	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector Intersection = ToolHelperFunctions::LinePlaneIntersectionCameraGroup(ToolViewportClient, CursorWorldPosition, CursorWorldDirection, TransformGroupActor);

	// We store an offset for each actor from the cursor so they stay at their relative location when moving
	SelectionOffset = Intersection - TransformGroupActor->GetActorLocation();
}

void FMoveMode::ToolUpdate()
{
	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector CursorIntersection = ToolHelperFunctions::LinePlaneIntersectionCameraGroup(ToolViewportClient, CursorWorldPosition, CursorWorldDirection, TransformGroupActor);

	TransformGroupActor->Modify();
	TransformGroupActor->SetActorLocation(CursorIntersection - SelectionOffset);
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

	// Begins the child transaction
	GEditor->BeginTransaction(FText());

	// Project the cursor from the screen to the world
	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector Intersection = ToolHelperFunctions::LinePlaneIntersectionCameraGroup(ToolViewportClient, CursorWorldPosition, CursorWorldDirection, TransformGroupActor);

	LastUpdateMouseRotVector = (Intersection - TransformGroupActor->GetActorLocation()).GetSafeNormal();
}

void FRotateMode::ToolUpdate()
{
	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FVector CursorIntersection = ToolHelperFunctions::LinePlaneIntersectionCameraGroup(ToolViewportClient, CursorWorldPosition, CursorWorldDirection, TransformGroupActor);

	FVector currentRotVector = (CursorIntersection - TransformGroupActor->GetActorLocation()).GetSafeNormal();

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

	TransformGroupActor->AddActorWorldRotation(AddRotation);

	LastUpdateMouseRotVector = (CursorIntersection - TransformGroupActor->GetActorLocation()).GetSafeNormal();
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

	// Begins the child transaction
	GEditor->BeginTransaction(FText());
	
	FIntPoint CursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	ActorScreenPosition = ToolHelperFunctions::ProjectWorldLocationToScreen(ToolViewportClient, TransformGroupActor->GetActorLocation());
	StartDistance = FVector2D::Distance((FVector2D)ActorScreenPosition, (FVector2D)CursorLocation);
}

void FScaleMode::ToolUpdate()
{
	FIntPoint CursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	float CurrentDistance = FVector2D::Distance((FVector2D)ActorScreenPosition, (FVector2D)CursorLocation);
	float NewScaleMultiplier = CurrentDistance / StartDistance;

	TransformGroupActor->Modify();
	TransformGroupActor->SetActorScale3D(FVector(NewScaleMultiplier));
}

void FScaleMode::ToolClose(bool Success)
{
	FBlenderToolMode::ToolClose(Success);
	UE_LOG(LogScaleTool, Verbose, TEXT("Closed"));
}
