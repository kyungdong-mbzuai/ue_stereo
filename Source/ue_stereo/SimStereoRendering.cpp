// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimStereoRendering.h"
#include "StereoRendering.h"
#include "SceneView.h"

FSimStereoRendering::FSimStereoRendering()
{
}

bool FSimStereoRendering::IsStereoEnabled() const
{
	return bStereoEnabled;
}

bool FSimStereoRendering::EnableStereo(bool bStereo)
{
	bStereoEnabled = bStereo;
	return bStereoEnabled;
}

void FSimStereoRendering::AdjustViewRect(const int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	// Split viewport into left/right halves for side-by-side stereo.
	SizeX = SizeX / 2;
	if (ViewIndex == eSSE_RIGHT_EYE)
	{
		X += SizeX;
	}
}

void FSimStereoRendering::CalculateStereoViewOffset(const int32 ViewIndex, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	// Eye offset is handled by USimLocalPlayer::GetProjectionData. No-op here.
}

FMatrix FSimStereoRendering::GetStereoProjectionMatrix(const int32 ViewIndex) const
{
	const float HalfHFovRad = FMath::DegreesToRadians(HorizontalFOV * 0.5f);
	const float HalfVFovRad = FMath::DegreesToRadians(VerticalFOV * 0.5f);
	const float NearPlane   = 10.0f; // 10 cm near plane

	// Standard perspective scale factors derived directly from half-angles.
	// XS = cot(HalfHFov),  YS = cot(HalfVFov)
	const float XS = 1.0f / FMath::Tan(HalfHFovRad);
	const float YS = 1.0f / FMath::Tan(HalfVFovRad);

	// Reversed-Z infinite projection matrix used by Unreal.
	return FMatrix(
		FPlane(XS,   0.0f, 0.0f,      0.0f),
		FPlane(0.0f, YS,   0.0f,      0.0f),
		FPlane(0.0f, 0.0f, 0.0f,      1.0f),
		FPlane(0.0f, 0.0f, NearPlane, 0.0f)
	);
}

void FSimStereoRendering::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	// No custom canvas setup required.
}
