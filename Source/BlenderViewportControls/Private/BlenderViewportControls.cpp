// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlenderViewportControls.h"
#include "BlenderViewportControlsEdMode.h"

#define LOCTEXT_NAMESPACE "FBlenderViewportControlsModule"

void FBlenderViewportControlsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorModeRegistry::Get().RegisterMode<FBlenderViewportControlsEdMode>(FBlenderViewportControlsEdMode::EM_BlenderViewportControlsEdModeId, LOCTEXT("BlenderViewportControlsEdModeName", "BlenderViewportControlsEdMode"), FSlateIcon(), true);
}

void FBlenderViewportControlsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FEditorModeRegistry::Get().UnregisterMode(FBlenderViewportControlsEdMode::EM_BlenderViewportControlsEdModeId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlenderViewportControlsModule, BlenderViewportControls)