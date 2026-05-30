// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "SimStereoRendering.h"
#include "SimLocalPlayer.generated.h"

/**
 * Custom LocalPlayer that overrides stereo projection data.
 * Uses OpenXR HMD pose injected each frame and a standalone FSimStereoRendering
 * for viewport splitting. GEngine->StereoRenderingDevice is NOT replaced so
 * OpenXR XRSystem stays active.
 */
UCLASS()
class UE_STEREO_API USimLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	USimLocalPlayer(const FObjectInitializer& ObjectInitializer);

	/**
	 * Override projection data to apply custom eye offset instead of HMD-driven offset.
	 * Uses BaseLocation / BaseRotation + IPD to build symmetric stereo view origins.
	 */
	virtual bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex = INDEX_NONE) const override;

	/** Center location used as the stereo baseline (driven by HMD pose or camera). */
	UPROPERTY(BlueprintReadWrite, Category = "Stereo")
	FVector BaseLocation = FVector::ZeroVector;

	/** Base rotation used for both eyes (driven by HMD pose or camera). */
	UPROPERTY(BlueprintReadWrite, Category = "Stereo")
	FRotator BaseRotation = FRotator::ZeroRotator;

	/** Inter-pupillary distance in cm (default 6.4 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (ClampMin = "0.0"))
	float IPD = 6.4f;

	/** Set to true once a valid HMD or camera transform has been written. */
	bool bBaseTransformSet = false;

	/**
	 * Standalone FSimStereoRendering used only for AdjustViewRect().
	 * NOT registered in GEngine->StereoRenderingDevice.
	 * Set by USimGameViewportClient::EnsureStereoDevice().
	 */
	TSharedPtr<FSimStereoRendering, ESPMode::ThreadSafe> CustomStereoDevice;
};
