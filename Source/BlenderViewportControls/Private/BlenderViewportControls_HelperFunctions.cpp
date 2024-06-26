// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_HelperFunctions.h"
#include "ViewportWorldInteraction.h"
#include "BlenderViewportControlsEdMode.h"
#include "BlenderViewportControls_Tools.h"
#include "CanvasTypes.h"
#include "EngineUtils.h"
#include "EditorModeManager.h"
#include "Components/LineBatchComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Selection.h"


TTuple<FVector, FVector> ToolHelperFunctions::GetCursorWorldPosition(FEditorViewportClient* InViewportClient)
{
	FSceneView* View = GetSceneView(InViewportClient);
	FIntPoint MousePosition = InViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	FVector CursorWorldPosition, CursorWorldDirection;
	View->DeprojectFVector2D(MousePosition, CursorWorldPosition, CursorWorldDirection);
	
	return TTuple<FVector, FVector>(CursorWorldPosition, CursorWorldDirection);
}

TTuple<FVector, FVector> ToolHelperFunctions::ProjectScreenPositionToWorld(FEditorViewportClient* InViewportClient, const FIntPoint& InScreenPosition)
{
	FSceneView* View = GetSceneView(InViewportClient);

	FVector CursorWorldPosition, CursorWorldDirection;
	View->DeprojectFVector2D(InScreenPosition, CursorWorldPosition, CursorWorldDirection);

	return TTuple<FVector, FVector>(CursorWorldPosition, CursorWorldDirection);
}

FVector ToolHelperFunctions::LinePlaneIntersectionFromCamera(FEditorViewportClient* InViewportClient, const FLinePlaneIntersectionHelper& InHelper)
{
	FVector OutIntersection;
	float T;

	// TODO?: This 100000 should probably be replaced with the actual distance + some extra between the cursor Pos and ObjectPosition
	UKismetMathLibrary::LinePlaneIntersection_OriginNormal(InHelper.TraceStartLocation,
		InHelper.TraceStartLocation + InHelper.TraceDirection * 10000000,
		InHelper.PlaneOrigin,
		InHelper.PlaneNormal,
		T,
		OutIntersection);

	return OutIntersection;
}

FIntPoint ToolHelperFunctions::ProjectWorldLocationToScreen(FEditorViewportClient* InViewportClient, FVector InWorldSpaceLocation, bool InClampValues)
{
	FSceneView* View = GetSceneView(InViewportClient);
	
	FVector2D OutScreenPos;
	FMatrix const ViewProjectionMatrix = View->ViewMatrices.GetViewProjectionMatrix();
	View->ProjectWorldToScreen(InWorldSpaceLocation, View->UnscaledViewRect, ViewProjectionMatrix, OutScreenPos);
	
	//Clamp Values because ProjectWorldToScreen can give you negative values...
	if (InClampValues)
	{
		FIntPoint ViewportResolution = InViewportClient->Viewport->GetSizeXY();
		OutScreenPos.X = FMath::Clamp((int32)OutScreenPos.X, 0, ViewportResolution.X);
		OutScreenPos.Y = FMath::Clamp((int32)OutScreenPos.Y, 0, ViewportResolution.Y);
	}

	return FIntPoint(OutScreenPos.X, OutScreenPos.Y);
}

FSceneView* ToolHelperFunctions::GetSceneView(FEditorViewportClient* InViewportClient)
{
	FViewportCursorLocation MousePosition = InViewportClient->GetCursorWorldLocationFromMousePos();
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewportClient->Viewport,
		InViewportClient->GetScene(),
		InViewportClient->EngineShowFlags));

	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);

	return View;
}

FBlenderViewportControlsEdMode* ToolHelperFunctions::GetEdMode()
{
	return (FBlenderViewportControlsEdMode*)GLevelEditorModeTools().GetActiveMode(FBlenderViewportControlsEdMode::EM_BlenderViewportControlsEdModeId);
}

ATransformGroupActor* ToolHelperFunctions::GetTransformGroupActor()
{
	return GetEdMode()->GetTransformGroupActor();
}

FVector ToolHelperFunctions::GetAverageLocation(const TArray<AActor*>& SelectedActors)
{
	FVector AverageLocation = FVector::ZeroVector;
	for (AActor* Actor : SelectedActors)
	{
		AverageLocation += Actor->GetActorLocation();
	}

	return AverageLocation / SelectedActors.Num();
}

void ToolHelperFunctions::DrawAxisLine(const UWorld* InWorld, const FVector& InLineOrigin, const FVector& InLineDirection, const FLinearColor& InLineColor)
{
	const float LineThickness = 3.f;
	const float LifeTime = 1.f;
	const float LineLength = 10000.f;

	FVector LineStart = InLineOrigin + (InLineDirection * LineLength);
	FVector LineEnd = InLineOrigin + (-InLineDirection * LineLength);

	InWorld->LineBatcher->DrawLine(LineStart, LineEnd, InLineColor, 0, LineThickness, LifeTime);
}

void ToolHelperFunctions::DrawDashedLine(FCanvas* InCanvas, const FVector& InLineStart, const FVector& InLineEnd, const float InLineThickness, const float InDashSize, const FLinearColor& InLineColor)
{
	FBatchedElements* BatchedElements = InCanvas->GetBatchedElements(FCanvas::ET_Line);
	FHitProxyId HitProxyId = InCanvas->GetHitProxyId();

	// Draw multiple lines between start and end point so it looks dashed
	{
		FVector LineDir = InLineEnd - InLineStart;
		float LineLeft = (InLineEnd - InLineStart).Size();
		if (LineLeft)
		{
			LineDir /= LineLeft;
		}

		const int32 nLines = FMath::CeilToInt(LineLeft / (InDashSize * 2));
		const FVector Dash = (InDashSize * LineDir);

		FVector DrawStart = InLineStart;
		while (LineLeft > InDashSize)
		{
			const FVector DrawEnd = DrawStart + Dash;

			BatchedElements->AddLine(DrawStart, DrawEnd, InLineColor, HitProxyId, InLineThickness);

			LineLeft -= 2 * InDashSize;
			DrawStart = DrawEnd + Dash;
		}
		if (LineLeft > 0.0f)
		{
			const FVector DrawEnd = InLineEnd;
			BatchedElements->AddLine(DrawStart, DrawEnd, InLineColor, HitProxyId, InLineThickness);
		}
	}
}

/**
 * [ Copy pasted from ActorFactory.cpp ]
 * 
 * Find am alignment transform for the specified actor rotation, given a model-space axis to align, and a world space normal to align to.
 * This function attempts to find a 'natural' looking rotation by rotating around a local pitch axis, and a world Z. Rotating in this way
 * should retain the roll around the model space axis, removing rotation artifacts introduced by a simpler quaternion rotation.
 */
FQuat ToolHelperFunctions::FindActorAlignmentRotation(const FQuat& InActorRotation, const FVector& InModelAxis, const FVector& InWorldNormal)
{
	FVector TransformedModelAxis = InActorRotation.RotateVector(InModelAxis);

	const auto InverseActorRotation = InActorRotation.Inverse();
	const auto DestNormalModelSpace = InverseActorRotation.RotateVector(InWorldNormal);

	FQuat DeltaRotation = FQuat::Identity;

	const float VectorDot = InWorldNormal | TransformedModelAxis;
	if (1.f - FMath::Abs(VectorDot) <= KINDA_SMALL_NUMBER)
	{
		if (VectorDot < 0.f)
		{
			// Anti-parallel
			return InActorRotation * FQuat::FindBetween(InModelAxis, DestNormalModelSpace);
		}
	}
	else
	{
		const FVector Z(0.f, 0.f, 1.f);

		// Find a reference axis to measure the relative pitch rotations between the source axis, and the destination axis.
		FVector PitchReferenceAxis = InverseActorRotation.RotateVector(Z);
		if (FMath::Abs(FVector::DotProduct(InModelAxis, PitchReferenceAxis)) > 0.7f)
		{
			PitchReferenceAxis = DestNormalModelSpace;
		}

		// Find a local 'pitch' axis to rotate around
		const FVector OrthoPitchAxis = FVector::CrossProduct(PitchReferenceAxis, InModelAxis);
		const float Pitch = FMath::Acos(PitchReferenceAxis | DestNormalModelSpace) - FMath::Acos(PitchReferenceAxis | InModelAxis);//FMath::Asin(OrthoPitchAxis.Size());

		DeltaRotation = FQuat(OrthoPitchAxis.GetSafeNormal(), Pitch);
		DeltaRotation.Normalize();

		// Transform the model axis with this new pitch rotation to see if there is any need for yaw
		TransformedModelAxis = (InActorRotation * DeltaRotation).RotateVector(InModelAxis);

		const float ParallelDotThreshold = 0.98f; // roughly 11.4 degrees (!)
		if (!FVector::Coincident(InWorldNormal, TransformedModelAxis, ParallelDotThreshold))
		{
			const float Yaw = FMath::Atan2(InWorldNormal.X, InWorldNormal.Y) - FMath::Atan2(TransformedModelAxis.X, TransformedModelAxis.Y);

			// Rotation axis for yaw is the Z axis in world space
			const FVector WorldYawAxis = (InActorRotation * DeltaRotation).Inverse().RotateVector(Z);
			DeltaRotation *= FQuat(WorldYawAxis, -Yaw);
		}
	}

	return InActorRotation * DeltaRotation;
}
