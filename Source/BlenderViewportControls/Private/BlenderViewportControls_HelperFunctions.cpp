// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls_HelperFunctions.h"
#include "ViewportWorldInteraction.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Selection.h"


TTuple<FVector, FVector> TransformHelperFunctions::GetCursorWorldPosition(class FEditorViewportClient* InViewportClient)
{
	FViewportCursorLocation MousePosition = InViewportClient->GetCursorWorldLocationFromMousePos();
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		InViewportClient->Viewport,
		InViewportClient->GetScene(),
		InViewportClient->EngineShowFlags));

	FSceneView* View = InViewportClient->CalcSceneView(&ViewFamily);

	FVector CursorWorldPosition, CursorWorldDirection;
	View->DeprojectFVector2D(MousePosition.GetCursorPos(), CursorWorldPosition, CursorWorldDirection);

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