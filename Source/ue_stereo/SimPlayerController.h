// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SimPlayerController.generated.h"

/**
 * PlayerController that updates USimLocalPlayer's BaseLocation and BaseRotation
 * every tick using the active camera's world transform.
 */
UCLASS()
class UE_STEREO_API ASimPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASimPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void OnMouseClickedCapture();
	void OnEscapePressed();
	void SetMouseFree();
	void SetMouseCaptured();

	void OnIPDIncrease();
	void OnIPDDecrease();
	void OnToggleStereoWindow();
	void OnToggleVRMode();

	bool bMouseCaptured = false;
	FVector2D LastMousePos = FVector2D::ZeroVector;

	// Mouse rotation sensitivity (degrees per pixel).
	float MouseSensitivity = 0.15f;

	// IPD adjustment step per key press (cm).
	static constexpr float IPDStep = 0.1f;
};
