// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class TransformHelperFunctions
{
public:
	static TTuple<FVector, FVector> GetCursorWorldPosition(class FEditorViewportClient* InViewportClient);
	static FVector LinePlaneIntersectionCameraObject(class FEditorViewportClient* InViewportClient, FVector InCursorWorldPos, FVector InCursorWorldDir);
};