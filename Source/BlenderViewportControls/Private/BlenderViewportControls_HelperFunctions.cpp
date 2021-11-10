// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_HelperFunctions.h"
#include "ViewportWorldInteraction.h"
#include "BlenderViewportControlsEdMode.h"
#include "BlenderViewportControls_Tools.h"
#include "EngineUtils.h"
#include "EditorModeManager.h"
#include "Components/LineBatchComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Selection.h"


TTuple<FVector, FVector> ToolHelperFunctions::GetCursorWorldPosition(class FEditorViewportClient* InViewportClient)
{
	FSceneView* View = GetSceneView(InViewportClient);
	FIntPoint MousePosition = InViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	FVector CursorWorldPosition, CursorWorldDirection;
	View->DeprojectFVector2D(MousePosition, CursorWorldPosition, CursorWorldDirection);
	
	return TTuple<FVector, FVector>(CursorWorldPosition, CursorWorldDirection);
}

TTuple<FVector, FVector> ToolHelperFunctions::ProjectScreenPositionToWorld(class FEditorViewportClient* InViewportClient, const FIntPoint& InScreenPosition)
{
	FSceneView* View = GetSceneView(InViewportClient);

	FVector CursorWorldPosition, CursorWorldDirection;
	View->DeprojectFVector2D(InScreenPosition, CursorWorldPosition, CursorWorldDirection);

	return TTuple<FVector, FVector>(CursorWorldPosition, CursorWorldDirection);
}

FVector ToolHelperFunctions::LinePlaneIntersectionFromCamera(class FEditorViewportClient* InViewportClient, const FLinePlaneIntersectionHelper& InHelper)
{
	FVector OutIntersection;
	float T;

	// TODO?: This 100000 should probably be replaced with the actual distance + some extra between the cursor Pos and ObjectPosition
	UKismetMathLibrary::LinePlaneIntersection_OriginNormal(InHelper.TraceStartLocation, InHelper.TraceStartLocation + (InHelper.TraceDirection * 10000000),
		InHelper.PlaneOrigin, InHelper.PlaneNormal, T, OutIntersection);

	return OutIntersection;
}

FIntPoint ToolHelperFunctions::ProjectWorldLocationToScreen(class FEditorViewportClient* InViewportClient, FVector InWorldSpaceLocation, bool InClampValues)
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

FSceneView* ToolHelperFunctions::GetSceneView(class FEditorViewportClient* InViewportClient)
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

class ATransformGroupActor* ToolHelperFunctions::GetTransformGroupActor()
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
