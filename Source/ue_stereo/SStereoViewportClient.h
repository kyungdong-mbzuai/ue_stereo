// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"
#include "SStereoViewportClient.generated.h"

// Independent viewport client for the stereo output window.
// Builds its own FSceneViewFamilyContext with two eye views and renders
// directly via GetRendererModule().BeginRenderingViewFamily().
// This path is completely separate from GEngine->StereoRenderingDevice.
UCLASS()
class UE_STEREO_API USStereoViewportClient : public UGameViewportClient
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

	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

private:
	// Persistent view states for temporal AA and other per-eye effects.
	FSceneViewStateReference ViewStateLeft;
	FSceneViewStateReference ViewStateRight;
};
