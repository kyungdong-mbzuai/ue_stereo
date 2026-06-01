// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Application/SlateApplication.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class UStereoViewportClient;
class FSceneViewport;
class UWorld;

struct FStereoWindowSettings
{
	int32 MonitorId = 1;
	int32 Width = 3840;
	int32 Height = 1080;
};

// Independent stereo output window.
// Owns a custom viewport client and an FSceneViewport.
// Renders its own scene via FSceneViewFamilyContext -- completely independent
// of GEngine->StereoRenderingDevice and the main viewport pipeline.
//
// Usage:
//   SAssignNew(StereoWindow, SStereoWindow);
//   StereoWindow->Open(World, OwnerActor, Settings);
//   StereoWindow->Tick();   // once per frame
//   StereoWindow->Close();
class SStereoWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStereoWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SStereoWindow();

	// Open the stereo window, spawn the camera, and start rendering.
	void Open(UWorld* World, AActor* Owner, const FStereoWindowSettings& InSettings);

	// Close the window and destroy all owned resources.
	void Close();

	bool IsOpen() const { return bWindowOpen; }

	// Call once per frame to trigger the independent scene render.
	void Tick();

	// Move/resize the OS window to a specific monitor.
	void MoveWindowToMonitor(int32 MonitorId, int32 Width, int32 Height);

	// Set the world-space pose of the stereo camera.
	void SetCameraPose(const FVector& Location, const FRotator& Rotation);

	// Access the viewport client to update camera pose or IPD externally.
	UStereoViewportClient* GetViewportClient() const { return ViewportClient; }

private:
	static FVector2D GetMonitorOrigin(int32 MonitorId);

	TSharedPtr<class SViewport>        ViewportWidget;
	TSharedPtr<SWindow>                OsWindow;
	TSharedPtr<FSceneViewport>         SceneViewport;

	TObjectPtr<UStereoViewportClient> ViewportClient;

	bool bWindowOpen = false;
};
