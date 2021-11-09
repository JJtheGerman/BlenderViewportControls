// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

struct FLinePlaneCameraHelper
{
	FVector TraceStartLocation;
	FVector TraceDirection;

	FVector PlaneOrigin;
	FVector PlaneNormal;
};

struct FAxisLineDrawHelper
{
	FAxisLineDrawHelper(const FVector& InLineDirection, const FLinearColor& InLinearColor)
		: LineDirection(InLineDirection), LineColor(InLinearColor) {};

	FVector LineDirection;
	FLinearColor LineColor;
};

class ToolHelperFunctions
{
public:
	static TTuple<FVector, FVector> GetCursorWorldPosition(class FEditorViewportClient* InViewportClient);
	static TTuple<FVector, FVector> ProjectScreenPositionToWorld(class FEditorViewportClient* InViewportClient, const FIntPoint& InScreenPosition);

	/** Trace from cursor to InTransformGroupActor and get a plane intersection where the plane normal is based on the camera forward vector */
	static FVector LinePlaneIntersectionFromCamera(class FEditorViewportClient* InViewportClient, const FLinePlaneCameraHelper& InLinePlaneCameraHelper);

	/** InClampValues will clamp values so they can't be negative. Otherwise it is possible to have values that are outside of the viewport */
	static FIntPoint ProjectWorldLocationToScreen(class FEditorViewportClient* InViewportClient, FVector InWorldSpaceLocation, bool InClampValues = false);

	static FSceneView* GetSceneView(class FEditorViewportClient* InViewportClient);
	static class FBlenderViewportControlsEdMode* GetEdMode();
	static class ATransformGroupActor* GetTransformGroupActor();
	static FVector GetAverageLocation(const TArray<AActor*>& SelectedActors);
	static void DrawAxisLine(const UWorld* InWorld, const FVector& InLineOrigin, const FVector& InLineDirection, const FLinearColor& InLineColor);
	static void DrawDashedLine(FCanvas* InCanvas, const FVector& InLineStart, const FVector& InLineEnd, const float InLineThickness = 2.5f, const float InDashSize = 10.f, const FLinearColor& InLineColor = FLinearColor::White);
};