// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimGameInstance.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"

void USimGameInstance::Init()
{
	Super::Init();

	// Try to enable stereo immediately (works for standalone builds).
	EnableStereo();

	// Also register for the engine loop complete callback to catch cases
	// where the StereoRenderingDevice is set after GameInstance::Init().
	FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &USimGameInstance::EnableStereo);
}

void USimGameInstance::Shutdown()
{
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	Super::Shutdown();
}

void USimGameInstance::EnableStereo()
{
	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GameInstance::EnableStereo called"));

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GameInstance::EnableStereo - GEngine is null, aborting"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GameInstance::EnableStereo - StereoRenderingDevice already valid: %s"),
		GEngine->StereoRenderingDevice.IsValid() ? TEXT("YES") : TEXT("NO"));

	// Install our custom software stereo device if no other device is present.
	if (!GEngine->StereoRenderingDevice.IsValid())
	{
		SimStereoRenderingDevice = MakeShareable(new FSimStereoRendering());
		GEngine->StereoRenderingDevice = SimStereoRenderingDevice;
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GameInstance::EnableStereo - FSimStereoRendering installed as StereoRenderingDevice"));
	}

	if (GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->EnableStereo(true);
		const bool bEnabled = GEngine->StereoRenderingDevice->IsStereoEnabled();
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] GameInstance::EnableStereo - EnableStereo(true) called. IsStereoEnabled=%s"),
			bEnabled ? TEXT("YES") : TEXT("NO"));
	}
}
