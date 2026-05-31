// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStereoViewportClient.h"
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
	// Do NOT call EnableStereoRendering(true) here.
	// Doing so registers this SViewport with GEngine->StereoRenderingDevice,
	// which causes the HMD device to resize the VR render buffer every frame
	// (e.g. 3840x1080 -> 3648x1968), overflowing our ViewRects and altering
	// the render target format — causing color/shadow differences vs the main viewport.
	// Stereo is handled entirely inside Draw() via FSceneViewInitOptions.StereoPass.
	UE_LOG(LogTemp, Log, TEXT("[SStereoViewportClient] InitStereoRendering complete (manual stereo, no SViewport registration)"));
}

void USStereoViewportClient::SetCameraTransform(const FVector& Location, const FRotator& Rotation)
{
	CameraLocation = Location;
	CameraRotation = Rotation;
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
	// Both eyes are marked PRIMARY so each gets its own AddCombineLUTPass call.
	// Marking the right eye as SECONDARY would cause the engine to reuse the
	// primary view's cached tonemapping LUT (ViewState->PrevFrameViewInfo.TonemappingLUT),
	// which is null on the first frame, triggering:
	//   Assertion failed: Inputs.ColorGradingTexture [PostProcessTonemap.cpp:573]
	// SBS split is handled entirely via ViewRect, so eSSP_PRIMARY is safe for both.
	InitOptions.StereoPass = EStereoscopicPass::eSSP_PRIMARY;

	// Apply camera post-process settings so tone-mapping, bloom, etc. match the main viewport.
	InitOptions.OverrideFarClippingPlaneDistance = CamInfo.OrthoFarClipPlane;

	FSceneView* View = new FSceneView(InitOptions);
	View->bIsGameView = true;

	// Fix exposure: disable auto-exposure history (EyeAdaptation ShowFlag is off)
	// and use manual exposure so the stereo window matches the main viewport.
	// Without this override the renderer falls back to a default (dark) adaptation.
	View->FinalPostProcessSettings.AutoExposureMethod           = EAutoExposureMethod::AEM_Manual;
	View->FinalPostProcessSettings.bOverride_AutoExposureMethod = true;
	// AutoExposureBias left at its inherited value from OverridePostProcessSettings
	// (camera component / post-process volume); do NOT force 0 here.

	// Inject the camera component's post-process blend chain.
	View->OverridePostProcessSettings(CamInfo.PostProcessSettings, CamInfo.PostProcessBlendWeight);

	ViewFamily.Views.Add(View);
	return View;
}

void USStereoViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	UWorld* CurrentWorld = TargetWorld.Get();
	if (!CurrentWorld || !CurrentWorld->Scene)
	{
		return;
	}

	const FIntPoint RenderSize = InViewport->GetSizeXY();
	if (RenderSize.X == 0 || RenderSize.Y == 0)
	{
		return;
	}

	FMinimalViewInfo CamInfo;
	CamInfo.Location = CameraLocation;
	CamInfo.Rotation = CameraRotation;
	CamInfo.FOV      = CameraFOV;

	// Copy ShowFlags from MirroredSettings (pushed by SimGameViewportClient::Draw()
	// each frame) so lighting, shadow, and feature flags match the main viewport exactly.
	FEngineShowFlags ShowFlags = MirroredSettings.ShowFlags;

	// Stereo-path overrides: keep PostProcessing, Tonemapper, and ColorGrading
	// enabled so AddCombineLUTPass runs for the primary (left) eye and the
	// tonemapper converts linear HDR scene color to display space.
	// Disabling PostProcessing sends scene color through DeviceEncodingOnly
	// with no tonemapper, which outputs raw linear values and looks dark.
	// EyeAdaptation is disabled because our ViewState has no adaptation history;
	// manual exposure set in AddEyeView() keeps the brightness stable instead.
	ShowFlags.SetStereoRendering(true);
	ShowFlags.SetEyeAdaptation(false);
	ShowFlags.SetBloom(false);
	ShowFlags.SetMotionBlur(false);


	FSceneViewFamily::ConstructionValues CVS(InViewport, CurrentWorld->Scene, ShowFlags);
	CVS.SetRealtimeUpdate(true);
	CVS.SetTime(FGameTime::GetTimeSinceAppStart());
	CVS.bResolveScene = true;

	FSceneViewFamilyContext ViewFamily(CVS);

	// Mirror HDR and display gamma from MirroredSettings (pushed by SimGameViewportClient::Draw()).
	// This avoids direct GEngine / main-window access inside the stereo draw path.
	const bool  bMainIsHDR      = MirroredSettings.bIsHDR;
	const float MainDisplayGamma = MirroredSettings.DisplayGamma;

	UE_LOG(LogTemp, VeryVerbose, TEXT("[SStereoViewportClient] MainWindow HDR=%s DisplayGamma=%.4f"),
		bMainIsHDR ? TEXT("true") : TEXT("false"), MainDisplayGamma);

	ViewFamily.bIsHDR = bMainIsHDR;

	// Apply the main viewport display gamma to the stereo scene viewport so
	// PostProcessDeviceEncodingOnly uses the same InvDisplayGamma as the main viewport.
	if (FSceneViewport* StereoVP = static_cast<FSceneViewport*>(Viewport))
	{
		StereoVP->SetGammaOverride(MainDisplayGamma);
	}

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

	// Left eye (negative IPD offset)
	AddEyeView(ViewFamily, ViewStateLeft,  CamInfo, RenderSize, 0, -HalfIPD, PerEyeHFov, PerEyeVFov, nullptr);
	// Right eye (positive IPD offset)
	AddEyeView(ViewFamily, ViewStateRight, CamInfo, RenderSize, 1, +HalfIPD, PerEyeHFov, PerEyeVFov, nullptr);

	GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
}
