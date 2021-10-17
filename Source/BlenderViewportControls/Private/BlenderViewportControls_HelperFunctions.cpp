// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_HelperFunctions.h"
#include "ViewportWorldInteraction.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Selection.h"


TTuple<FVector, FVector> TransformHelperFunctions::GetCursorWorldPosition(class FEditorViewportClient* InViewportClient)
{
	FSceneView* View = GetSceneView(InViewportClient);
	FIntPoint MousePosition = InViewportClient->GetCursorWorldLocationFromMousePos().GetCursorPos();

	FVector CursorWorldPosition, CursorWorldDirection;
	View->DeprojectFVector2D(MousePosition, CursorWorldPosition, CursorWorldDirection);

	return TTuple<FVector, FVector>(CursorWorldPosition, CursorWorldDirection);
}

FVector TransformHelperFunctions::LinePlaneIntersectionCameraObject(class FEditorViewportClient* InViewportClient, FVector InCursorWorldPos, FVector InCursorWorldDir)
{
	FVector OutIntersection;
	float T;
	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (AActor* SelectedActor = Cast<AActor>(SelectedActors->GetSelectedObject(0)))
	{
		FVector ScreenSpacePlaneNormal = InViewportClient->GetViewRotation().Vector();
		// TODO?: This 100000 should probably be replaced with the actual distance + some extra between the cursor Pos and ObjectPosition
		UKismetMathLibrary::LinePlaneIntersection_OriginNormal(InCursorWorldPos, InCursorWorldPos + (InCursorWorldDir * 100000),
			SelectedActor->GetActorLocation(), ScreenSpacePlaneNormal, T, OutIntersection);
	}

	return OutIntersection;
}

FIntPoint TransformHelperFunctions::ProjectWorldLocationToScreen(class FEditorViewportClient* InViewportClient, FVector InWorldSpaceLocation, bool InClampValues)
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

FSceneView* TransformHelperFunctions::GetSceneView(class FEditorViewportClient* InViewportClient)
{
	FViewportCursorLocation MousePosition = InViewportClient->GetCursorWorldLocationFromMousePos();
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewportClient->Viewport,
		InViewportClient->GetScene(),
		InViewportClient->EngineShowFlags));

	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);

	return View;
}
