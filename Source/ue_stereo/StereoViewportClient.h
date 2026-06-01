// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "StereoViewportClient.generated.h"

// Rendering settings mirrored from the main game viewport each frame.
// SimGameViewportClient::Draw() fills this and pushes it to SStereoViewportClient
// so the stereo window never needs to access GEngine or the main window directly.
struct FMirroredRenderSettings
{
	bool             bIsHDR        = false;
	float            DisplayGamma  = 2.2f;
	FEngineShowFlags ShowFlags     = FEngineShowFlags(ESFIM_Game);
};

// Independent viewport client for the stereo output window.
// Builds its own FSceneViewFamilyContext with two eye views and renders
// directly via GetRendererModule().BeginRenderingViewFamily().
// This path is completely separate from GEngine->StereoRenderingDevice.
UCLASS()
class UE_STEREO_API UStereoViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	// Camera pose — set externally each frame (e.g. from HMD head tracking).
	FVector  CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	float    CameraFOV      = 90.0f;

	// World reference — set explicitly because this client has no GameInstance.
	TWeakObjectPtr<UWorld> TargetWorld;

	// OS window reference — used to enable stereo on the SViewport widget.
	TWeakPtr<SWindow> TargetWindow;

	// Inter-pupillary distance in world units (cm).
	float IPD = 0.0f;

	// Update camera pose from external source (e.g. HMD head tracking).
	void SetCameraTransform(const FVector& Location, const FRotator& Rotation);

	// Enable stereo rendering on the given SViewport widget.
	void InitStereoRendering(TSharedPtr<SViewport> InViewportWidget);

	// Rendering settings pushed from SimGameViewportClient::Draw() each frame.
	// Avoids direct GEngine / main-window access inside the stereo draw path.
	FMirroredRenderSettings MirroredSettings;

	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

private:
	// Persistent view states for temporal AA and other per-eye effects.
	FSceneViewStateReference ViewStateLeft;
	FSceneViewStateReference ViewStateRight;
};
