// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimGameViewportClient.h"
#include "Engine/Engine.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "RenderingThread.h"

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

	EnsureStereoDevice();
	LoadStereoWindowConfig();
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

void USimGameViewportClient::EnsureStereoDevice()
{
	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] EnsureStereoDevice - GEngine is null"));
		return;
	}

	if (!GEngine->StereoRenderingDevice.IsValid())
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
