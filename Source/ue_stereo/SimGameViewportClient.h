// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "SimStereoRendering.h"
#include "SStereoWindow.h"
#include "SimGameViewportClient.generated.h"

/**
 * Custom GameViewportClient for the stereo simulation project.
 * Supports side-by-side stereo rendering via a dedicated OS window (SStereoWindow).
 */
UCLASS()
class UE_STEREO_API USimGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

	// Retrieve the current HMD head pose via OpenXR.
	// Returns false when no XR tracking system is available.
	bool GetHMDHeadPose(FQuat& OutOrientation, FVector& OutPosition) const;

	// Open the stereo output window on the configured monitor.
	void OpenStereoWindow();

	// Close the stereo output window.
	void CloseStereoWindow();

	// Toggle stereo output window open/closed.
	void ToggleStereoWindow();

	// Switch to custom (FSimStereoRendering) stereo device.
	void SetVRMode_CustomStereo();

	// Switch to OpenXR HMD stereo device.
	void SetVRMode_OpenXR();

	// Toggle between CustomStereo and OpenXR VR modes.
	void ToggleVRMode();

protected:
	virtual void BeginDestroy() override;

private:
	void EnsureStereoDevice();
	void LoadStereoWindowConfig();
	
	bool bCustomStereo = false;
	bool bStereoEnabledOnViewport = false;
	bool bViewportWindowResizable  = false;
	TSharedPtr<FSimStereoRendering, ESPMode::ThreadSafe> SimStereoRenderingDevice;

	// Stereo output window (separate OS window receiving scene render texture).
	TSharedPtr<SStereoWindow> StereoWindow;

	// Config: stereo window display settings.
	int32 StereoWindowMonitorId = 1;
	int32 StereoWindowWidth     = 3840;
	int32 StereoWindowHeight    = 1080;
};

