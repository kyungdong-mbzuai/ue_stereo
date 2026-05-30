// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "SimLocalPlayer.generated.h"

/**
 * Custom LocalPlayer that overrides stereo projection data
 * to replace HMD-driven eye offsets with a manually controlled IPD offset.
 */
UCLASS()
class UE_STEREO_API USimLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	/**
	 * Override projection data to apply custom eye offset instead of HMD-driven offset.
	 * Uses BaseLocation / BaseRotation + IPD to build symmetric stereo view origins.
	 */
	virtual bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex = INDEX_NONE) const override;

	/** Center location used as the stereo baseline (e.g. camera component world location). */
	UPROPERTY(BlueprintReadWrite, Category = "Stereo")
	FVector BaseLocation = FVector::ZeroVector;

	/** Base rotation used for both eyes. */
	UPROPERTY(BlueprintReadWrite, Category = "Stereo")
	FRotator BaseRotation = FRotator::ZeroRotator;

	/** Inter-pupillary distance in cm (default 6.4 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stereo", meta = (ClampMin = "0.0"))
	float IPD = 6.4f;

	/** Set to true once Tick has written a valid camera transform. */
	bool bBaseTransformSet = false;
};
