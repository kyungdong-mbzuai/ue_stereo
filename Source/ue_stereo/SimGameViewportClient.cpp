// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimGameViewportClient.h"
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

	EnsureStereoDevice();
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

void USimGameViewportClient::EnsureStereoDevice()
{
	if (! bCustomStereo)
	{
		return;
	}

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureStereoDevice - GEngine is null"));
		return;
	}

	if (GEngine->StereoRenderingDevice != SimStereoRenderingDevice)
	{
		SimStereoRenderingDevice = MakeShareable(new FSimStereoRendering());
		GEngine->StereoRenderingDevice = SimStereoRenderingDevice;
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureStereoDevice - FSimStereoRendering installed"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureStereoDevice - StereoRenderingDevice already valid, skipping install"));
	}

	GEngine->StereoRenderingDevice->EnableStereo(true);
	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureStereoDevice - EnableStereo(true) called. IsStereoEnabled=%s"),
		GEngine->StereoRenderingDevice->IsStereoEnabled() ? TEXT("YES") : TEXT("NO"));

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

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] StereoWindow opened - Monitor=%d Size=%dx%d"),
		Settings.MonitorId, Settings.Width, Settings.Height);
}

void USimGameViewportClient::CloseStereoWindow()
{
	if (StereoWindow.IsValid() && StereoWindow->IsOpen())
	{
		StereoWindow->Close();
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
	EnsureStereoDevice();

	FVector CurrentHMDPosition;
	FQuat CurrentHMDOrientation;
	GetHMDHeadPose(CurrentHMDOrientation, CurrentHMDPosition);

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GetHMDHeadPose - Orientation=%s Position=%s"),
		*CurrentHMDOrientation.ToString(), *CurrentHMDPosition.ToString());

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

	if (!bStereoEnabledOnViewport)
	{
		bStereoEnabledOnViewport = true;
		const bool bDeviceValid   = GEngine && GEngine->StereoRenderingDevice.IsValid();
		const bool bStereoEnabled = bDeviceValid && GEngine->StereoRenderingDevice->IsStereoEnabled();
		const bool bStereo3D      = GEngine && GEngine->IsStereoscopic3D(InViewport);
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] ViewportClient::Draw(first) - StereoDevice valid=%s, IsStereoEnabled=%s, IsStereoscopic3D=%s"),
			bDeviceValid   ? TEXT("YES") : TEXT("NO"),
			bStereoEnabled ? TEXT("YES") : TEXT("NO"),
			bStereo3D      ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] ViewportClient::Draw(first) - IsStereoRenderingAllowed=%s, EngineShowFlags.StereoRendering=%s"),
			InViewport && InViewport->IsStereoRenderingAllowed() ? TEXT("YES") : TEXT("NO"),
			EngineShowFlags.StereoRendering ? TEXT("YES") : TEXT("NO"));
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

	SimStereoRenderingDevice = MakeShareable(new FSimStereoRendering());
	GEngine->StereoRenderingDevice = SimStereoRenderingDevice;
	GEngine->StereoRenderingDevice->EnableStereo(true);
	EngineShowFlags.SetStereoRendering(true);
	bCustomStereo = true;

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] SetVRMode_CustomStereo - FSimStereoRendering installed"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(10, 3.0f, FColor::Yellow, TEXT("[VR Mode] Custom Stereo"));
	}
}

void USimGameViewportClient::SetVRMode_OpenXR()
{
	if (!GEngine)
	{
		return;
	}

	// Release custom device so the engine falls back to the OpenXR HMD device.
	GEngine->StereoRenderingDevice = nullptr;
	SimStereoRenderingDevice.Reset();
	EngineShowFlags.SetStereoRendering(true);
	bCustomStereo = false;

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] SetVRMode_OpenXR - Switched to OpenXR HMD device"));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(10, 3.0f, FColor::Green, TEXT("[VR Mode] OpenXR HMD"));
	}
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
