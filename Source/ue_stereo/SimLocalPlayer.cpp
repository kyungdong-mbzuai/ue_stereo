// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimLocalPlayer.h"
#include "Engine/Engine.h"
#include "StereoRendering.h"
#include "SceneView.h"

USimLocalPlayer::USimLocalPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IPD = 0.0f;
}

bool USimLocalPlayer::GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex) const
{
	// 1. Call Super with INDEX_NONE (mono) to get the camera's correct base transform
	//    and its natural symmetric projection matrix (derived from actual camera FOV).
	//    This avoids IStereoRendering::GetStereoProjectionMatrix() replacing the projection.
	const bool bSuccess = Super::GetProjectionData(Viewport, ProjectionData, INDEX_NONE);

	if (!bSuccess)
	{
		return false;
	}

	// Use standalone CustomStereoDevice instead of GEngine->StereoRenderingDevice.
	// This keeps OpenXR active as GEngine's device while we control the split here.
	if (!CustomStereoDevice.IsValid() || !CustomStereoDevice->IsStereoEnabled())
	{
		return bSuccess;
	}

	if (StereoViewIndex == INDEX_NONE)
	{
		return bSuccess;
	}

	// 2. Split the viewport rect for side-by-side stereo via our standalone device.
	const FIntRect BaseRect = ProjectionData.GetViewRect();
	int32  X     = BaseRect.Min.X;
	int32  Y     = BaseRect.Min.Y;
	uint32 SizeX = (uint32)BaseRect.Width();
	uint32 SizeY = (uint32)BaseRect.Height();
	CustomStereoDevice->AdjustViewRect(StereoViewIndex, X, Y, SizeX, SizeY);
	ProjectionData.SetViewRectangle(FIntRect(X, Y, X + (int32)SizeX, Y + (int32)SizeY));

	// Fix projection matrix aspect ratio:
	// Super(INDEX_NONE) built the ProjectionMatrix for the full viewport width.
	// Now that we halved the width, the horizontal scale (M[0][0]) must double
	// so the vertical FOV stays correct and horizontal FOV matches the half viewport.
	FMatrix& PM = ProjectionData.ProjectionMatrix;
	PM.M[0][0] *= 2.0f;

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GetProjectionData eye=%d rect=(%d,%d,%d,%d) bBaseSet=%s"),
		StereoViewIndex, X, Y, X + (int32)SizeX, Y + (int32)SizeY,
		bBaseTransformSet ? TEXT("YES") : TEXT("NO"));

	// 3. Apply IPD eye offset once a valid HMD/camera transform has been set.
	//    The ProjectionMatrix remains the camera's natural symmetric matrix.
	if (!bBaseTransformSet)
	{
		return bSuccess;
	}

	const FVector RightVector = BaseRotation.Quaternion().GetAxisY();

	if (StereoViewIndex == eSSE_LEFT_EYE)
	{
		ProjectionData.ViewOrigin = BaseLocation - (RightVector * (IPD * 0.5f));
	}
	else if (StereoViewIndex == eSSE_RIGHT_EYE)
	{
		ProjectionData.ViewOrigin = BaseLocation + (RightVector * (IPD * 0.5f));
	}

	// Must match ULocalPlayer::GetProjectionData's ViewRotationMatrix construction:
	// FInverseRotationMatrix * axis-swap (UE world: X=fwd,Y=right,Z=up -> view: X=right,Y=up,Z=fwd)
	ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(BaseRotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	return bSuccess;
}

