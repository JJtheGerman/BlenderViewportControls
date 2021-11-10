// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_Tools.h"
#include "BlenderViewportControls_HelperFunctions.h"
#include "ViewportWorldInteraction.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/LineBatchComponent.h"
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

	// Create a new GroupTransform for this tool
	GroupTransform = MakeShared<FGroupTransform>();

	USelection* CurrentSelection = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*CurrentSelection); Iter; ++Iter)
	{
		if (AActor* LevelActor = Cast<AActor>(*Iter))
		{
			SelectionInfos.Add(FSelectionToolHelper(LevelActor, LevelActor->GetTransform()));

			FIntPoint ActorScreenLocation = ToolHelperFunctions::ProjectWorldLocationToScreen(ToolViewportClient, LevelActor->GetActorLocation());
			FIntPoint MousePosition = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();
			FIntPoint ScreenSpaceOffset = MousePosition - ActorScreenLocation;

			GroupTransform->AddChild(LevelActor, ScreenSpaceOffset);
		}
	}
	GroupTransform->FinishSetup(ToolViewportClient);

	// Start Parent Transaction
	GEditor->BeginTransaction(OperationName);
}

void FBlenderToolMode::ToolClose(bool Success /*= true*/)
{
	// Reset the selection outline color
	GEditor->SetSelectionOutlineColor(DefaultSelectionOutlineColor);

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

void FBlenderToolMode::CalculateAxisLock()
{
	// There is nothing to do when we aren't locking anything.
	if (AxisLockHelper.CurrentLockedAxis == EToolAxisLock::NONE) { return; }

	TArray<EToolAxisLock> LockedAxes;
	TArray<FVector> LockVectors;
	TArray<FLinearColor> LineColors;

	// Dual Axis locks need to draw two lines. If a user presses Shift + X for example it will draw the Y and Z axes instead.	
	if (AxisLockHelper.IsDualAxisLock)
	{
		if (AxisLockHelper.CurrentLockedAxis == X)
		{
			LockedAxes.Add(EToolAxisLock::Z);
			LockedAxes.Add(EToolAxisLock::Y);
		}
		else if (AxisLockHelper.CurrentLockedAxis == Y)
		{
			LockedAxes.Add(EToolAxisLock::X);
			LockedAxes.Add(EToolAxisLock::Z);
		}
		else if (AxisLockHelper.CurrentLockedAxis == Z)
		{
			LockedAxes.Add(EToolAxisLock::Y);
			LockedAxes.Add(EToolAxisLock::X);
		}
	}
	else
	{
		LockedAxes.Add(AxisLockHelper.CurrentLockedAxis);
	}

	bool IsWorldSpace = AxisLockHelper.IsWorldSpace ? true : false;
	for (const auto& Axis : LockedAxes)
	{
		FVector LockVector;

		switch (Axis)
		{
		case X:
			LockVector = IsWorldSpace ? FVector::ForwardVector : GroupTransform->GetLocalForwardVector();
			AxisLineDrawHelper.Add(FAxisLineDrawHelper(LockVector, FLinearColor::Red));
			AxisLockHelper.LockVector = LockVector;
			if (AxisLockHelper.IsDualAxisLock)
			{
				AxisLockHelper.LockPlaneNormal = IsWorldSpace ? FVector::UpVector : GroupTransform->GetLocalUpVector();
			}

			break;
		case Y:
			LockVector = IsWorldSpace ? FVector::RightVector : GroupTransform->GetLocalRightVector();
			AxisLineDrawHelper.Add(FAxisLineDrawHelper(LockVector, FLinearColor::Green));
			AxisLockHelper.LockVector = LockVector;
			if (AxisLockHelper.IsDualAxisLock)
			{
				AxisLockHelper.LockPlaneNormal = IsWorldSpace ? FVector::ForwardVector : GroupTransform->GetLocalForwardVector();
			}
			break;
		case Z:
			LockVector = IsWorldSpace ? FVector::UpVector : GroupTransform->GetLocalUpVector();
			AxisLineDrawHelper.Add(FAxisLineDrawHelper(LockVector, FLinearColor::Blue));
			AxisLockHelper.LockVector = LockVector;
			if (AxisLockHelper.IsDualAxisLock)
			{
				AxisLockHelper.LockPlaneNormal = IsWorldSpace ? FVector::RightVector : GroupTransform->GetLocalRightVector();
			}
			break;
		}
	}
}

void FBlenderToolMode::DrawAxisLocks()
{
	const UWorld* World = ToolViewportClient->GetWorld();
	for (const auto& AxisLine : AxisLineDrawHelper)
	{
		ToolHelperFunctions::DrawAxisLine(World, AxisLockHelper.TransformWhenLocked.GetLocation(), AxisLine.LineDirection, AxisLine.LineColor);
	}
}

void FBlenderToolMode::SetAxisLock(const EToolAxisLock& InAxisToLock, bool bDualAxis)
{
	// Remove the lines we are currently drawing
	AxisLineDrawHelper.Empty();

	AxisLockHelper.CurrentLockedAxis = InAxisToLock;
	AxisLockHelper.IsDualAxisLock = bDualAxis;
	AxisLockHelper.TransformWhenLocked = GetGroupTransform()->GetParentTransform();

	// Single selections can toggle between local and world space, but multi selections should always only have local space locking
	AxisLockHelper.IsWorldSpace = IsSingleSelection() ? !AxisLockHelper.IsWorldSpace : true;
}




void FBlenderToolMode::AddSnapOffset(const float InOffset)
{
	SnapOffset += InOffset;
}

/**
 * Move Tool Implementation
 */
void FMoveMode::ToolBegin()
{
	UE_LOG(LogMoveTool, Verbose, TEXT("Begin"));

	// Begins the child transaction
	GEditor->BeginTransaction(FText());
}

void FMoveMode::ToolUpdate()
{
	CalculateAxisLock();

	FIntPoint ScreenSpaceOffsetLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos() + GroupTransform->GetScreenSpaceOffset();
	
	// Trace from the cursor onto a plane and get the intersection
	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::ProjectScreenPositionToWorld(ToolViewportClient, ScreenSpaceOffsetLocation);
	FVector TransformWorldPosition = WorldLocDir.Get<0>();
	FVector TransformWorldDirection = WorldLocDir.Get<1>();

	FLinePlaneIntersectionHelper LinePlaneCameraHelper;
	LinePlaneCameraHelper.PlaneOrigin = GroupTransform->GetOriginLocation();
	LinePlaneCameraHelper.TraceStartLocation = TransformWorldPosition;
	LinePlaneCameraHelper.TraceDirection = TransformWorldDirection;
	LinePlaneCameraHelper.PlaneNormal = GetCameraForwardVector();
	if (AxisLockHelper.IsDualAxisLock && AxisLockHelper.CurrentLockedAxis != NONE)
	{
		LinePlaneCameraHelper.PlaneNormal = AxisLockHelper.LockPlaneNormal;
	}

	FVector NewLocation = ToolHelperFunctions::LinePlaneIntersectionFromCamera(ToolViewportClient, LinePlaneCameraHelper);

	// Single Axis Locking
	FVector LockedLocation = NewLocation;
	if (!AxisLockHelper.IsDualAxisLock && AxisLockHelper.CurrentLockedAxis != NONE)
	{
		LockedLocation = UKismetMathLibrary::FindClosestPointOnLine(NewLocation, GroupTransform->GetOriginLocation(), AxisLockHelper.LockVector);
	}

	// Draw the visual axis locking lines in the viewport
	DrawAxisLocks();

	// Surface Snap mode
	if (IsSurfaceSnapping())
	{
		TArray<AActor*> IgnoredActors = GroupTransform->GetAllChildActors();
		for (auto& Child : GroupTransform->GetChildren())
		{
			FIntPoint ChildScreenLocation = Child.ScreenSpaceOffset + ScreenSpaceOffsetLocation;
			TTuple<FVector, FVector> Test = ToolHelperFunctions::ProjectScreenPositionToWorld(ToolViewportClient, ChildScreenLocation);
			FVector TraceStart = Test.Get<0>();
			FVector TraceEnd = Test.Get<1>() * 10000.f;
			FHitResult OutHit;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActors(IgnoredActors);
			if (ToolViewportClient->GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
			{
				if (OutHit.bBlockingHit)
				{
					FVector NewLocWithSnapOffset = OutHit.ImpactPoint + (OutHit.ImpactNormal * SnapOffset);
					Child.Actor->SetActorLocation(NewLocWithSnapOffset);
					Child.Actor->SetActorRotation(UKismetMathLibrary::MakeRotFromZ(OutHit.ImpactNormal));
				}
			}
		}
	}
	else
	{
		// Set the final position
		GroupTransform->SetLocation(LockedLocation);
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

	// Begins the child transaction
	GEditor->BeginTransaction(FText());

	// Project the cursor from the screen to the world
	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FLinePlaneIntersectionHelper Helper;
	Helper.TraceStartLocation = CursorWorldPosition;
	Helper.TraceDirection = CursorWorldDirection;
	Helper.PlaneOrigin = GroupTransform->GetOriginLocation();
	Helper.PlaneNormal = GetCameraForwardVector();
	FVector Intersection = ToolHelperFunctions::LinePlaneIntersectionFromCamera(ToolViewportClient, Helper);

	LastUpdateMouseRotVector = (Intersection - GroupTransform->GetOriginLocation()).GetSafeNormal();
}

void FRotateMode::ToolUpdate()
{
	CalculateAxisLock();

	TTuple<FVector, FVector> WorldLocDir = ToolHelperFunctions::GetCursorWorldPosition(ToolViewportClient);
	FVector CursorWorldPosition = WorldLocDir.Get<0>();
	FVector CursorWorldDirection = WorldLocDir.Get<1>();

	// Trace from the cursor onto a plane and get the intersection
	FLinePlaneIntersectionHelper Helper;
	Helper.TraceStartLocation = CursorWorldPosition;
	Helper.TraceDirection = CursorWorldDirection;
	Helper.PlaneOrigin = GroupTransform->GetOriginLocation();
	Helper.PlaneNormal = GetCameraForwardVector();
	FVector CursorIntersection = ToolHelperFunctions::LinePlaneIntersectionFromCamera(ToolViewportClient, Helper);

	FVector currentRotVector = (CursorIntersection - GroupTransform->GetOriginLocation()).GetSafeNormal();

	// Gives us the degrees between the cursor starting position and end position in -180 - 180;
	float degreesBetweenVectors = UKismetMathLibrary::DegAcos(FVector::DotProduct(currentRotVector, LastUpdateMouseRotVector));

	// First check if the camera is looking down or up and flips. Otherwise dragging the mouse left would rotate the object
	// to the right whenever we are looking from below. (Hard to explain, just uncomment the first "if" statement to see the behavior.
	if (ToolViewportClient->GetViewRotation().Vector().Z > 0)
	{
		degreesBetweenVectors = degreesBetweenVectors * -1.f;
	}
	if (FVector::CrossProduct(currentRotVector, LastUpdateMouseRotVector).Z < 0.f)
	{
		degreesBetweenVectors = -degreesBetweenVectors;
	}

	// Set the rotation axis. Its determined by the active axis lock. 
	// Some extra inverting of the axis needs to be done depending on camera position for the mouse interaction to work as a "human" would expect it to.
	FVector LockedRotationAxis;
	LockedRotationAxis = AxisLockHelper.LockVector * (FVector::DotProduct(AxisLockHelper.LockVector, GetCameraForwardVector()) < 0 ? -1.f : 1.f);
	if (AxisLockHelper.CurrentLockedAxis == NONE)
	{
		LockedRotationAxis = ToolViewportClient->GetViewRotation().Vector();
	}

	FRotator AddRotation = UKismetMathLibrary::RotatorFromAxisAndAngle(LockedRotationAxis, degreesBetweenVectors);

	if (LastCursorLocation == ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos())
	{
		AddRotation = FRotator::ZeroRotator;
	}	

	FVector Origin = GroupTransform->GetOriginLocation();
	GroupTransform->AddRotation(AddRotation);

	LastUpdateMouseRotVector = (CursorIntersection - GroupTransform->GetOriginLocation()).GetSafeNormal();
	LastCursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	DrawAxisLocks();
}

void FRotateMode::ToolClose(bool Success)
{
	FBlenderToolMode::ToolClose(Success);

	UE_LOG(LogRotateTool, Verbose, TEXT("Closed"));
}

void FRotateMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FVector2D MousePosition = ViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();
	const FVector LineStart = FVector(MousePosition.X, MousePosition.Y, 0.f);
	const FVector LineEnd = FVector(GroupTransform->GetOriginScreenLocation());
	
	// Draw a dashed line between the origin and the cursor
	ToolHelperFunctions::DrawDashedLine(Canvas, LineStart, LineEnd);
}

void FRotateMode::SetAxisLock(const EToolAxisLock& InAxisToLock, bool bDualAxis)
{
	FBlenderToolMode::SetAxisLock(InAxisToLock, bDualAxis);

	// The rotate tool doesn't support two axis rotation because it doesn't make sense
	AxisLockHelper.IsDualAxisLock = false;
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

	ActorScreenPosition = ToolHelperFunctions::ProjectWorldLocationToScreen(ToolViewportClient, GroupTransform->GetOriginLocation());
	StartDistance = FVector2D::Distance((FVector2D)ActorScreenPosition, (FVector2D)CursorLocation);
}

void FScaleMode::ToolUpdate()
{
	CalculateAxisLock();

	FIntPoint CursorLocation = ToolViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	float CurrentDistance = FVector2D::Distance((FVector2D)ActorScreenPosition, (FVector2D)CursorLocation);
	float NewScaleMultiplier = CurrentDistance / StartDistance;

	GroupTransform->SetScale(FVector(NewScaleMultiplier), AxisLockHelper.LockVector, !AxisLockHelper.isLocked());
	DrawAxisLocks();
}

void FScaleMode::ToolClose(bool Success)
{
	FBlenderToolMode::ToolClose(Success);
	UE_LOG(LogScaleTool, Verbose, TEXT("Closed"));
}

void FScaleMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FVector2D MousePosition = ViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();
	const FVector LineStart = FVector(MousePosition.X, MousePosition.Y, 0.f);
	const FVector LineEnd = FVector(GroupTransform->GetOriginScreenLocation());

	// Draw a dashed line between the origin and the cursor
	ToolHelperFunctions::DrawDashedLine(Canvas, LineStart, LineEnd);
}




void FGroupTransform::SetScale(const FVector& InNewScale, const FVector& ScaleAxis, const bool bUniformScale)
{
	for (const FChildTransform& Child : Children)
	{
		FVector BiasScale;
		if (!bUniformScale)
		{
			FVector newScaleAxis = ScaleAxis;

			float X_Alpha = FMath::Abs(FVector::DotProduct(newScaleAxis, Child.Actor->GetActorForwardVector()));
			float Y_Alpha = FMath::Abs(FVector::DotProduct(newScaleAxis, Child.Actor->GetActorRightVector()));
			float Z_Alpha = FMath::Abs(FVector::DotProduct(newScaleAxis, Child.Actor->GetActorUpVector()));

			float x, y, z;
			x = FMath::Lerp(1.f, InNewScale.X, X_Alpha);
			y = FMath::Lerp(1.f, InNewScale.Y, Y_Alpha);
			z = FMath::Lerp(1.f, InNewScale.Z, Z_Alpha);

			BiasScale = FVector(x, y, z);
		}
		else
		{
			BiasScale = InNewScale;
		}

		FTransform ScaleTransform = FTransform(FQuat::Identity, FVector::ZeroVector, BiasScale);
		FTransform ScaleAroundParent = FTransform(-Parent.GetLocation()) * ScaleTransform * FTransform(Parent.GetLocation());

		FTransform NewChildTransform = Child.ChildOriginalTransform * ScaleAroundParent;

		Child.Actor->Modify();
		Child.Actor->SetActorTransform(NewChildTransform);
	}
}

void FGroupTransform::SetAverageLocation()
{
	FVector averageLocation = FVector::ZeroVector;
	for (auto& Child : Children)
	{
		averageLocation += Child.Actor->GetActorLocation();
	}
	averageLocation /= Children.Num();
	Parent.SetLocation(averageLocation);
}

void FGroupTransform::AddRotation(const FRotator& InAddRotation)
{
	FTransform RotationAroundParent = FTransform(-Parent.GetLocation()) * FTransform(InAddRotation.Quaternion()) * FTransform(Parent.GetLocation());
	for (auto& Child : Children)
	{
		FTransform RotatedTransform = Child.Actor->GetTransform() * RotationAroundParent;

		Child.Actor->Modify();
		Child.Actor->SetActorTransform(RotatedTransform);
	}
}

void FGroupTransform::SetLocation(const FVector& InNewLocation)
{
	Parent.SetLocation(InNewLocation);

	for (auto& Child : Children)
	{
		FVector RelativeLocation = -Child.RelativeOffset + Parent.GetLocation();

		Child.Actor->Modify();
		Child.Actor->SetActorLocation(RelativeLocation);
	}
}

void FGroupTransform::AddChild(AActor* NewChild, const FIntPoint& InScreenspaceOffset)
{
	Children.Add(FChildTransform(NewChild, InScreenspaceOffset));
}

void FGroupTransform::FinishSetup(FEditorViewportClient* InViewportClient)
{
	SetAverageLocation();
	Parent.SetRotation(Children[0].ChildOriginalTransform.GetRotation());

	for (auto& Child : Children)
	{
		Child.RelativeOffset = Parent.GetLocation() - Child.ChildOriginalTransform.GetLocation();
	}

	// Calculate the screen space offset between the transform origin and the cursor
	FIntPoint CursorPosition = InViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();
	FIntPoint TransformScreenPosition = ToolHelperFunctions::ProjectWorldLocationToScreen(InViewportClient, GetOriginLocation());

	ScreenSpaceParentCursorOffset = TransformScreenPosition - CursorPosition;
	CurrentWorld = InViewportClient->GetWorld();
	ParentOriginalTransform = Parent;

	// Origin location in screen-space used for line drawing
	OriginScreenLocation = ToolHelperFunctions::ProjectWorldLocationToScreen(InViewportClient, Parent.GetLocation());
}

TArray<AActor*> FGroupTransform::GetAllChildActors()
{
	TArray<AActor*> OutActors;
	for (auto& Child : Children)
	{
		OutActors.Add(Child.Actor);
	}

	return OutActors;
}