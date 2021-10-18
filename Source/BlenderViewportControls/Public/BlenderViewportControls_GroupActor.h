// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlenderViewportControls_GroupActor.generated.h"

UCLASS(hidedropdown, MinimalAPI, notplaceable, NotBlueprintable, Transient)
class ATransformGroupActor : public AActor
{
	GENERATED_BODY()
public:
	ATransformGroupActor() {};

	////~ Begin AActor interface
	virtual bool IsEditorOnly() const final
	{
		return true;
	}
	////~ End AActor interface
};