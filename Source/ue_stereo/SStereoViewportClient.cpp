// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStereoViewportClient.h"
#include "SStereoWindowCamera.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "EngineModule.h"
#include "RendererInterface.h"
#include "CanvasTypes.h"
#include "LegacyScreenPercentageDriver.h"
#include "Widgets/SViewport.h"
#include "Slate/SceneViewport.h"

void USStereoViewportClient::InitStereoRendering(TSharedPtr<SViewport> InViewportWidget)
{
	// Do NOT touch GEngine->StereoRenderingDevice.
	// Stereo for this window is handled entirely inside Draw().
	if (InViewportWidget.IsValid() && !InViewportWidget->IsStereoRenderingAllowed())
	{
		InViewportWidget->EnableStereoRendering(true);
	}

	UE_LOG(LogTemp, Log, TEXT("[SStereoViewportClient] InitStereoRendering complete"));
}

void USStereoViewportClient::SetCameraTransform(const FVector& Location, const FRotator& Rotation)
{
	if (StereoCamera.IsValid())
	{
		StereoCamera->SetActorLocationAndRotation(Location, Rotation);
	}
}

// Build one eye view and add it to the ViewFamily.
// ViewIndex 0 = left eye, 1 = right eye.
static FSceneView* AddEyeView(
	FSceneViewFamilyContext& ViewFamily,
	FSceneViewStateReference& InViewState,
	const FMinimalViewInfo& CamInfo,
	const FIntPoint& RenderSize,
	int32 ViewIndex,
	float IPDHalfOffset,
	float HFovDeg,
	float VFovDeg,
	AActor* ViewActor)
{
	const int32 HalfW  = RenderSize.X / 2;
	const int32 OffsetX = (ViewIndex == 1) ? HalfW : 0;
	const FIntRect ViewRect(OffsetX, 0, OffsetX + HalfW, RenderSize.Y);

	// Eye origin: shift along the camera's right axis by the IPD half-offset.
	const FVector CamRight  = CamInfo.Rotation.RotateVector(FVector::RightVector);
	const FVector EyeOrigin = CamInfo.Location + CamRight * IPDHalfOffset;

	// View rotation matrix: UE world axes -> render axes (Y-right, Z-up, X-forward).
	const FMatrix ViewRotMatrix = FInverseRotationMatrix(CamInfo.Rotation)
		* FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

	// Symmetric reversed-Z infinite projection.
	const float XS = 1.0f / FMath::Tan(FMath::DegreesToRadians(HFovDeg * 0.5f));
	const float YS = 1.0f / FMath::Tan(FMath::DegreesToRadians(VFovDeg * 0.5f));
	const FMatrix ProjMatrix(
		FPlane(XS,  0.0f, 0.0f, 0.0f),
		FPlane(0.0f, YS,  0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, GNearClippingPlane, 0.0f));

	FSceneViewInitOptions InitOptions;
	InitOptions.ViewFamily             = &ViewFamily;
	InitOptions.ViewActor              = ViewActor;
	InitOptions.SceneViewStateInterface = InViewState.GetReference();
	InitOptions.SetViewRectangle(ViewRect);
	InitOptions.ViewOrigin             = EyeOrigin;
	InitOptions.ViewRotationMatrix     = ViewRotMatrix;
	InitOptions.ProjectionMatrix       = ProjMatrix;
	InitOptions.StereoPass             = (ViewIndex == 0)
		? EStereoscopicPass::eSSP_PRIMARY
		: EStereoscopicPass::eSSP_SECONDARY;

	FSceneView* View = new FSceneView(InitOptions);
	View->bIsGameView = true;

	ViewFamily.Views.Add(View);
	return View;
}

void USStereoViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	UWorld* CurrentWorld = TargetWorld.Get();
	if (!CurrentWorld || !CurrentWorld->Scene || !StereoCamera.IsValid())
	{
		return;
	}

	UCameraComponent* CamComp = StereoCamera->CameraComponent;
	if (!CamComp)
	{
		return;
	}

	const FIntPoint RenderSize = InViewport->GetSizeXY();
	if (RenderSize.X == 0 || RenderSize.Y == 0)
	{
		return;
	}

	FMinimalViewInfo CamInfo;
	CamComp->GetCameraView(0.0f, CamInfo);

	// Show flags: game view with SBS stereo.
	// Post-process / tone-mapper disabled — no ULocalPlayer::CalcSceneView path
	// is used, so the ColorGradingLUT is not generated.
	FEngineShowFlags ShowFlags(ESFIM_Game);
	ShowFlags.SetStereoRendering(true);
	ShowFlags.SetPostProcessing(false);
	ShowFlags.SetTonemapper(false);
	ShowFlags.SetColorGrading(false);
	ShowFlags.SetEyeAdaptation(false);

	FSceneViewFamily::ConstructionValues CVS(InViewport, CurrentWorld->Scene, ShowFlags);
	CVS.SetRealtimeUpdate(true);
	CVS.SetTime(FGameTime::GetTimeSinceAppStart());

	FSceneViewFamilyContext ViewFamily(CVS);
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

	// Per-eye H-FOV = half of the full camera FOV (SBS layout).
	// V-FOV is derived from the per-eye (half-width x height) aspect ratio.
	const float PerEyeHFov = CamInfo.FOV * 0.5f;
	const float PerEyeAspect = (RenderSize.Y > 0)
		? (static_cast<float>(RenderSize.X) * 0.5f) / static_cast<float>(RenderSize.Y)
		: 1.0f;
	const float PerEyeVFov = FMath::RadiansToDegrees(
		2.0f * FMath::Atan(
			FMath::Tan(FMath::DegreesToRadians(PerEyeHFov * 0.5f)) / PerEyeAspect));

	const float HalfIPD = IPD * 0.5f;
	AActor* ViewActor   = StereoCamera.Get();

	// Left eye (negative IPD offset)
	AddEyeView(ViewFamily, ViewStateLeft,  CamInfo, RenderSize, 0, -HalfIPD, PerEyeHFov, PerEyeVFov, ViewActor);
	// Right eye (positive IPD offset)
	AddEyeView(ViewFamily, ViewStateRight, CamInfo, RenderSize, 1, +HalfIPD, PerEyeHFov, PerEyeVFov, ViewActor);

	GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
}
