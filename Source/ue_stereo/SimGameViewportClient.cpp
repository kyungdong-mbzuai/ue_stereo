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

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] ViewportClient::Init - StereoRenderingDevice valid: %s"),
		GEngine && GEngine->StereoRenderingDevice.IsValid() ? TEXT("YES") : TEXT("NO"));

	LoadStereoWindowConfig();
}

void USimGameViewportClient::BeginDestroy()
{
	// ReleaseResources calls Close() internally. Close() guards against Slate being
	// already shut down, but we still need to skip FlushRenderingCommands when the
	// render thread is no longer running (edge case during editor hot-reload / exit).
	if (StereoWindow.IsValid())
	{
		StereoWindow->Close();
		StereoWindow.Reset();
	}

	Super::BeginDestroy();
}

void USimGameViewportClient::LoadStereoWindowConfig()
{
	// In a packaged/standalone build FPaths::ProjectDir() points to the directory
	// containing the .uproject (or the game root next to the Binaries folder).
	// Prefer a Config.ini sitting next to the executable so operators can edit it
	// without touching the packaged content.  Fall back to the project Config dir
	// so the editor PIE workflow continues to work unchanged.
	const FString ExeDir        = FPaths::GetPath(FPlatformProcess::ExecutablePath());
	const FString ExeSideConfig = ExeDir / TEXT("Config/Config.ini");
	const FString ProjectConfig = FPaths::ProjectDir() / TEXT("Config/Config.ini");

	FString ConfigPath;
	if (FPaths::FileExists(ExeSideConfig))
	{
		ConfigPath = ExeSideConfig;
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] Using exe-side Config.ini: %s"), *ConfigPath);
	}
	else if (FPaths::FileExists(ProjectConfig))
	{
		ConfigPath = ProjectConfig;
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] Using project Config.ini: %s"), *ConfigPath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] Config.ini not found (checked %s and %s), using defaults"),
			*ExeSideConfig, *ProjectConfig);
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

void USimGameViewportClient::OpenStereoWindow(UWorld* InWorld, AActor* Owner)
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
	StereoWindow->Open(InWorld, Owner, Settings);

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
		UWorld* CurrentWorld = GetWorld();
		OpenStereoWindow(CurrentWorld, nullptr);
	}
}

void USimGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	FVector HeadLocation;
	FRotator HeadRotation;

	UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HeadRotation, HeadLocation);
	
	//UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GetHMDHeadPose - Orientation=%s Position=%s"),
	//	*HeadRotation.ToString(), *HeadLocation.ToString());

	// Inject OpenXR HMD pose into SimLocalPlayer so GetProjectionData uses it each frame.
	if (bCustomStereo)
	{
		ULocalPlayer* LP = GEngine ? GEngine->GetFirstGamePlayer(this) : nullptr;
		USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(LP);
		if (SimPlayer)
		{
			SimPlayer->BaseLocation      = HeadLocation;
			SimPlayer->BaseRotation      = HeadRotation;
			SimPlayer->bBaseTransformSet = true;
		}
	}

	if (GEngine)
	{
		const FRotator HMDRotator = HeadRotation;
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("[HMD] Orientation: P=%.2f Y=%.2f R=%.2f"),
				HMDRotator.Pitch, HMDRotator.Yaw, HMDRotator.Roll));
		GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Green,
			FString::Printf(TEXT("[HMD] Position: X=%.2f Y=%.2f Z=%.2f"),
				HeadLocation.X, HeadLocation.Y, HeadLocation.Z));
	}

	// NOTE: Do NOT call ViewportWidget->EnableStereoRendering(true) here.
	// Doing so registers the main SViewport with GEngine->StereoRenderingDevice (OpenXR HMD),
	// which causes the HMD to resize the VR buffer to its preferred resolution every frame
	// (e.g. 1168x560), conflicting with the stereo window (e.g. 2560x1080) and causing
	// repeated "Resizing VR buffer" log spam. Stereo is handled exclusively inside
	// SStereoViewportClient::Draw() via FSceneViewInitOptions, so no SViewport registration
	// is needed or desired.

	// Render scene first so the viewport render target is populated.
	Super::Draw(InViewport, SceneCanvas);

	// Tick the independent stereo window -- triggers its own scene render.
	if (StereoWindow.IsValid() && StereoWindow->IsOpen())
	{
		// Push main viewport rendering settings to the stereo viewport client
		// before Tick() so SStereoViewportClient::Draw() never accesses GEngine directly.
		USStereoViewportClient* StereoVC = StereoWindow->GetViewportClient();
		if (StereoVC)
		{
			FMirroredRenderSettings MirroredSettings;
			MirroredSettings.ShowFlags   = EngineShowFlags;
			MirroredSettings.DisplayGamma = InViewport ? InViewport->GetDisplayGamma() : 2.2f;

			TSharedPtr<SWindow> GameWindow = GetWindow();
			MirroredSettings.bIsHDR = GameWindow.IsValid() && GameWindow->GetIsHDR();

			StereoVC->MirroredSettings = MirroredSettings;
		}

		UWorld* CurrentWorld = GetWorld();
		APlayerController* PC = CurrentWorld ? CurrentWorld->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			// Base pose from player camera manager (world space)
			const FVector  BaseLocation = PC->PlayerCameraManager->GetCameraLocation();
			const FRotator BaseRotation = PC->PlayerCameraManager->GetCameraRotation();

			// HMD head pose is relative to the tracking origin (in UE units, cm)
			// Rotate the HMD offset into world space using the base camera rotation
			const FVector  WorldHeadOffset = BaseRotation.RotateVector(HeadLocation);
			const FVector  FinalLocation = BaseLocation + WorldHeadOffset;
			const FRotator FinalRotation = (BaseRotation.Quaternion() * HeadRotation.Quaternion()).Rotator();

			StereoWindow->SetCameraPose(FinalLocation, FinalRotation);
		}

		StereoWindow->Tick();
	}
}

void USimGameViewportClient::SetVRMode_CustomStereo()
{
	if (!GEngine)
	{
		return;
	}

	SimStereoRenderingDevice = MakeShareable(new FSimStereoRendering());
	SimStereoRenderingDevice->EnableStereo(true);
	bCustomStereo = true;
	EngineShowFlags.SetStereoRendering(true);

	// Replace GEngine->StereoRenderingDevice so the engine allocates a viewport
	// render target and populates it each frame — required for GetRenderTargetTexture()
	// to return a valid texture in standalone (no OpenXR) builds.
	OriginalStereoRenderingDevice = GEngine->StereoRenderingDevice;
	GEngine->StereoRenderingDevice = SimStereoRenderingDevice;

	// Share immediately with SimLocalPlayer.
	ULocalPlayer* LP = GEngine->GetFirstGamePlayer(this);
	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(LP);
	if (SimPlayer)
	{
		SimPlayer->CustomStereoDevice = SimStereoRenderingDevice;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] SetVRMode_CustomStereo - FSimStereoRendering set as GEngine->StereoRenderingDevice"));
	GEngine->AddOnScreenDebugMessage(10, 3.0f, FColor::Yellow, TEXT("[VR Mode] Custom Stereo"));
}

void USimGameViewportClient::SetVRMode_OpenXR()
{
	if (!GEngine)
	{
		return;
	}

	// Restore the original StereoRenderingDevice (OpenXR or null).
	GEngine->StereoRenderingDevice = OriginalStereoRenderingDevice;
	OriginalStereoRenderingDevice.Reset();

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
	EngineShowFlags.SetStereoRendering(false);

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] SetVRMode_OpenXR - GEngine->StereoRenderingDevice restored"));
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
