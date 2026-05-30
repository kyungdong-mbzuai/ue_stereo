// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimGameViewportClient.h"
#include "SimLocalPlayer.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "Engine/Engine.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "RenderingThread.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

void USimGameViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);

	EngineShowFlags.SetStereoRendering(true);

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] ViewportClient::Init - EngineShowFlags.StereoRendering set to true"));
	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] ViewportClient::Init - StereoRenderingDevice valid: %s"),
		GEngine && GEngine->StereoRenderingDevice.IsValid() ? TEXT("YES") : TEXT("NO"));

	LoadStereoWindowConfig();

	//bCustomStereo = true;
	//EnsureStereoDevice();
}

void USimGameViewportClient::BeginDestroy()
{
	// ReleaseResources calls Close() internally. Close() guards against Slate being
	// already shut down, but we still need to skip FlushRenderingCommands when the
	// render thread is no longer running (edge case during editor hot-reload / exit).
	if (StereoWindow.IsValid())
	{
		StereoWindow->ReleaseResources();
		StereoWindow.Reset();
	}

	Super::BeginDestroy();
}

void USimGameViewportClient::LoadStereoWindowConfig()
{
	const FString ConfigPath = FPaths::ProjectDir() / TEXT("Config/Config.ini");
	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] Config.ini not found at %s, using defaults"), *ConfigPath);
		return;
	}

	GConfig->GetInt(TEXT("StereoWindow"), TEXT("MonitorId"), StereoWindowMonitorId, ConfigPath);
	GConfig->GetInt(TEXT("StereoWindow"), TEXT("Width"),     StereoWindowWidth,     ConfigPath);
	GConfig->GetInt(TEXT("StereoWindow"), TEXT("Height"),    StereoWindowHeight,    ConfigPath);

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] StereoWindow config - Monitor=%d Size=%dx%d"),
		StereoWindowMonitorId, StereoWindowWidth, StereoWindowHeight);
}

bool USimGameViewportClient::GetHMDHeadPose(FQuat& OutOrientation, FVector& OutPosition) const
{
	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GetHMDHeadPose - XR system not available"));
		return false;
	}

	const bool bSuccess = GEngine->XRSystem->GetCurrentPose(
		IXRTrackingSystem::HMDDeviceId,
		OutOrientation,
		OutPosition);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GetHMDHeadPose - GetCurrentPose failed"));
		return bSuccess;
	}

	return bSuccess;
}

void USimGameViewportClient::EnsureCustomStereo()
{
	if (!bCustomStereo)
	{
		return;
	}

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureCustomStereo - GEngine is null"));
		return;
	}

	// Create standalone SimStereoRendering if not yet created.
	// GEngine->StereoRenderingDevice is intentionally NOT replaced so OpenXR XRSystem stays active.
	if (!SimStereoRenderingDevice.IsValid())
	{
		SimStereoRenderingDevice = MakeShareable(new FSimStereoRendering());
		SimStereoRenderingDevice->EnableStereo(true);
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureCustomStereo - standalone FSimStereoRendering created (GEngine device NOT replaced)"));
	}

	// Share the standalone device with SimLocalPlayer so GetProjectionData can use AdjustViewRect.
	ULocalPlayer* LP = GEngine->GetFirstGamePlayer(this);
	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(LP);
	if (SimPlayer && SimPlayer->CustomStereoDevice != SimStereoRenderingDevice)
	{
		SimPlayer->CustomStereoDevice = SimStereoRenderingDevice;
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureCustomStereo - CustomStereoDevice shared with SimLocalPlayer"));
	}

	EngineShowFlags.SetStereoRendering(true);
}

void USimGameViewportClient::OpenStereoWindow()
{
	if (StereoWindow.IsValid() && StereoWindow->IsOpen())
	{
		return;
	}

	if (!StereoWindow.IsValid())
	{
		StereoWindow = SNew(SStereoWindow);
	}

	FStereoWindowSettings Settings;
	Settings.MonitorId = StereoWindowMonitorId;
	Settings.Width     = StereoWindowWidth;
	Settings.Height    = StereoWindowHeight;
	StereoWindow->Open(Settings);

	SetVRMode_CustomStereo();

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] StereoWindow opened - Monitor=%d Size=%dx%d"),
		Settings.MonitorId, Settings.Width, Settings.Height);
}

void USimGameViewportClient::CloseStereoWindow()
{
	if (StereoWindow.IsValid() && StereoWindow->IsOpen())
	{
		StereoWindow->Close();
		SetVRMode_OpenXR();
		UE_LOG(LogTemp, Log, TEXT("[SimStereo] StereoWindow closed"));
	}
}

void USimGameViewportClient::ToggleStereoWindow()
{
	if (StereoWindow.IsValid() && StereoWindow->IsOpen())
	{
		CloseStereoWindow();
	}
	else
	{
		OpenStereoWindow();
	}
}

void USimGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	bool bNoSpectator = false;

	IXRTrackingSystem* XRSystem = GEngine ? GEngine->XRSystem.Get() : nullptr;
	if (bNoSpectator) {
		//ESpectatorScreenMode Mode = ESpectatorScreenMode::SingleEyeCroppedToFill;
		ESpectatorScreenMode Mode = ESpectatorScreenMode::Disabled;
		UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenMode(Mode);
	}

	EnsureCustomStereo();

	FVector CurrentHMDPosition;
	FQuat   CurrentHMDOrientation;
	const bool bHasPose = GetHMDHeadPose(CurrentHMDOrientation, CurrentHMDPosition);

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GetHMDHeadPose - Orientation=%s Position=%s"),
		*CurrentHMDOrientation.ToString(), *CurrentHMDPosition.ToString());

	// Inject OpenXR HMD pose into SimLocalPlayer so GetProjectionData uses it each frame.
	if (bHasPose && bCustomStereo)
	{
		ULocalPlayer* LP = GEngine ? GEngine->GetFirstGamePlayer(this) : nullptr;
		USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(LP);
		if (SimPlayer)
		{
			SimPlayer->BaseLocation      = CurrentHMDPosition;
			SimPlayer->BaseRotation      = CurrentHMDOrientation.Rotator();
			SimPlayer->bBaseTransformSet = true;
		}
	}

	if (GEngine)
	{
		const FRotator HMDRotator = CurrentHMDOrientation.Rotator();
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("[HMD] Orientation: P=%.2f Y=%.2f R=%.2f"),
				HMDRotator.Pitch, HMDRotator.Yaw, HMDRotator.Roll));
		GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Green,
			FString::Printf(TEXT("[HMD] Position: X=%.2f Y=%.2f Z=%.2f"),
				CurrentHMDPosition.X, CurrentHMDPosition.Y, CurrentHMDPosition.Z));
	}

	TSharedPtr<SViewport> ViewportWidget = GetGameViewportWidget();
	if (ViewportWidget.IsValid() && !ViewportWidget->IsStereoRenderingAllowed())
	{
		ViewportWidget->EnableStereoRendering(true);
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] ViewportClient::Draw - SViewport::EnableStereoRendering(true) called"));
	}


	// Render scene first so the viewport render target is populated.
	Super::Draw(InViewport, SceneCanvas);

	// Forward the scene render texture to the stereo output window when open.
	if (StereoWindow.IsValid() && StereoWindow->IsOpen())
	{
		FTextureRHIRef SceneRHITexture = InViewport->GetRenderTargetTexture();
		const FIntPoint TextureSize    = InViewport->GetSizeXY();

		if (SceneRHITexture.IsValid())
		{
			StereoWindow->UpdateSceneTexture(SceneRHITexture, TextureSize);

			static int32 FrameCounter = 0;
			if (FrameCounter++ % 60 == 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[SimStereo] Draw: StereoWindow Texture=%p Size=%dx%d"),
					SceneRHITexture.GetReference(), TextureSize.X, TextureSize.Y);
			}
		}
	}
}

void USimGameViewportClient::SetVRMode_CustomStereo()
{
	if (!GEngine)
	{
		return;
	}

	// Create standalone device. GEngine->StereoRenderingDevice is NOT replaced.
	SimStereoRenderingDevice = MakeShareable(new FSimStereoRendering());
	SimStereoRenderingDevice->EnableStereo(true);
	bCustomStereo = true;
	EngineShowFlags.SetStereoRendering(true);

	// Share immediately with SimLocalPlayer.
	ULocalPlayer* LP = GEngine->GetFirstGamePlayer(this);
	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(LP);
	if (SimPlayer)
	{
		SimPlayer->CustomStereoDevice = SimStereoRenderingDevice;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] SetVRMode_CustomStereo - standalone FSimStereoRendering created (OpenXR pose + SimRendering split)"));
	GEngine->AddOnScreenDebugMessage(10, 3.0f, FColor::Yellow, TEXT("[VR Mode] Custom Stereo (OpenXR pose + SimRendering)"));
}

void USimGameViewportClient::SetVRMode_OpenXR()
{
	if (!GEngine)
	{
		return;
	}

	// Clear standalone device and SimLocalPlayer reference.
	ULocalPlayer* LP = GEngine->GetFirstGamePlayer(this);
	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(LP);
	if (SimPlayer)
	{
		SimPlayer->CustomStereoDevice.Reset();
		SimPlayer->bBaseTransformSet = false;
	}

	SimStereoRenderingDevice.Reset();
	bCustomStereo = false;

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] SetVRMode_OpenXR - Reverted to full OpenXR control"));
	GEngine->AddOnScreenDebugMessage(10, 3.0f, FColor::Green, TEXT("[VR Mode] OpenXR HMD"));
}

void USimGameViewportClient::ToggleVRMode()
{
	if (bCustomStereo)
	{
		SetVRMode_OpenXR();
	}
	else
	{
		SetVRMode_CustomStereo();
	}
}
