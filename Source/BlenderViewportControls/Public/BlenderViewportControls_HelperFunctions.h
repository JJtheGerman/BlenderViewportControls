// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class TransformHelperFunctions
{
public:
	static TTuple<FVector, FVector> GetCursorWorldPosition(class FEditorViewportClient* InViewportClient);
	static FVector LinePlaneIntersectionCameraObject(class FEditorViewportClient* InViewportClient, FVector InCursorWorldPos, FVector InCursorWorldDir);
	/** InClampValues will clamp values so they can't be negative. Otherwise it is possible to have values that are outside of the viewport */
	static FIntPoint ProjectWorldLocationToScreen(class FEditorViewportClient* InViewportClient, FVector InWorldSpaceLocation, bool InClampValues = false);
	static FSceneView* GetSceneView(class FEditorViewportClient* InViewportClient);
};