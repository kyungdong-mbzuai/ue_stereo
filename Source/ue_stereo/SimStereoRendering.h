// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StereoRendering.h"

/**
 * Software stereo rendering implementation for side-by-side (3D TV) output.
 * No HMD required. Left eye occupies the left half, right eye the right half of the viewport.
 */
class FSimStereoRendering : public IStereoRendering
{
public:
	FSimStereoRendering();

	// IStereoRendering interface
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool bStereo = true) override;
	virtual void AdjustViewRect(const int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const int32 ViewIndex, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const int32 ViewIndex) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	/** Horizontal FOV used for the asymmetric projection matrix (degrees). */
	float HorizontalFOV = 90.0f;

	/** Vertical FOV used for the asymmetric projection matrix (degrees). */
	float VerticalFOV = 45.0f;

private:
	bool bStereoEnabled = false;
};
