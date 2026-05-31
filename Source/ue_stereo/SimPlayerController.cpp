// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimPlayerController.h"
#include "SimLocalPlayer.h"
#include "SimGameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"

ASimPlayerController::ASimPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ASimPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetMouseFree();

	//
	// STANDALONE GAME MODE:
	// 

	// Verify that the XR system was initialized by the engine.
	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		return;
	}


	// Enable HMD to receive tracking data.
	UHeadMountedDisplayFunctionLibrary::EnableHMD(true);
	GEngine->StereoRenderingDevice->EnableStereo(true);	


	// Use floor-level tracking origin for accurate coordinates.
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);

	// Disable stereo rendering so the main viewport is not split.
	if (1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SimStereo] PlayerController::BeginPlay - Disabling stereo rendering on main viewport"));
	
		if (GEngine->StereoRenderingDevice.IsValid())
		{
			GEngine->StereoRenderingDevice->EnableStereo(true);	
		}
	}
	if (0)
	{
		ESpectatorScreenMode Mode = ESpectatorScreenMode::SingleEyeCroppedToFill;
		//ESpectatorScreenMode Mode = ESpectatorScreenMode::Disabled;
		UHeadMountedDisplayFunctionLibrary::SetSpectatorScreenMode(Mode);
	}


	USimGameViewportClient* VPC = Cast<USimGameViewportClient>(GetWorld() ? GetWorld()->GetGameViewport() : nullptr);
	if (VPC)
	{
		//VPC->SetVRMode_CustomStereo();
	}
}

void ASimPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASimPlayerController::OnMouseClickedCapture);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASimPlayerController::OnEscapePressed);
		InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &ASimPlayerController::OnIPDIncrease);
		InputComponent->BindKey(EKeys::Equals, IE_Repeat, this, &ASimPlayerController::OnIPDIncrease);
		InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &ASimPlayerController::OnIPDDecrease);
		InputComponent->BindKey(EKeys::Hyphen, IE_Repeat, this, &ASimPlayerController::OnIPDDecrease);
		InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ASimPlayerController::OnToggleStereoWindow);
		InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ASimPlayerController::OnToggleVRMode);
	}
}

void ASimPlayerController::OnMouseClickedCapture()
{
	SetMouseCaptured();
}

void ASimPlayerController::OnEscapePressed()
{
	SetMouseFree();
}

void ASimPlayerController::OnToggleStereoWindow()
{
	USimGameViewportClient* VPC = Cast<USimGameViewportClient>(GetWorld() ? GetWorld()->GetGameViewport() : nullptr);
	if (VPC)
	{
		VPC->ToggleStereoWindow();
	}
}

void ASimPlayerController::OnToggleVRMode()
{
	USimGameViewportClient* VPC = Cast<USimGameViewportClient>(GetWorld() ? GetWorld()->GetGameViewport() : nullptr);
	if (!VPC)
	{
		return;
	}
	VPC->ToggleVRMode();
}

void ASimPlayerController::OnIPDIncrease()
{
	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(GetLocalPlayer());
	if (!SimPlayer)
	{
		return;
	}
	SimPlayer->IPD = FMath::Max(0.0f, SimPlayer->IPD + IPDStep);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("IPD: %.2f cm"), SimPlayer->IPD));
	}
}

void ASimPlayerController::OnIPDDecrease()
{
	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(GetLocalPlayer());
	if (!SimPlayer)
	{
		return;
	}
	SimPlayer->IPD = FMath::Max(0.0f, SimPlayer->IPD - IPDStep);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("IPD: %.2f cm"), SimPlayer->IPD));
	}
}

void ASimPlayerController::SetMouseFree()
{
	bMouseCaptured = false;
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASimPlayerController::SetMouseCaptured()
{
	bMouseCaptured = true;
	bShowMouseCursor = false;

	// Record current position so the first delta is zero.
	if (FSlateApplication::IsInitialized())
	{
		LastMousePos = FSlateApplication::Get().GetCursorPos();
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	InputMode.SetHideCursorDuringCapture(true);
	SetInputMode(InputMode);
}

void ASimPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMouseCaptured && FSlateApplication::IsInitialized())
	{
		const FVector2D CurrentPos = FSlateApplication::Get().GetCursorPos();
		const FVector2D Delta = CurrentPos - LastMousePos;
		LastMousePos = CurrentPos;

		if (!Delta.IsNearlyZero())
		{
			const FRotator Current = GetControlRotation();
			const float NewYaw   = Current.Yaw   + Delta.X * MouseSensitivity;
			const float NewPitch = FMath::ClampAngle(Current.Pitch - Delta.Y * MouseSensitivity, -89.0f, 89.0f);
			SetControlRotation(FRotator(NewPitch, NewYaw, 0.0f));
		}
	}

	USimLocalPlayer* SimPlayer = Cast<USimLocalPlayer>(GetLocalPlayer());
	if (!SimPlayer)
	{
		return;
	}

	if (!PlayerCameraManager)
	{
		return;
	}

	SimPlayer->BaseLocation = PlayerCameraManager->GetCameraLocation();
	SimPlayer->BaseRotation = PlayerCameraManager->GetCameraRotation();
	SimPlayer->bBaseTransformSet = true;
}
