// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "SStereoWindowCamera.generated.h"

// Camera actor owned by SStereoWindow.
// Rendered exclusively through the independent stereo pipeline in UStereoViewportClient.
UCLASS()
class UE_STEREO_API ASStereoWindowCamera : public AActor
{
	GENERATED_BODY()

public:
	ASStereoWindowCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StereoWindow")
	UCameraComponent* CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoWindow")
	float FieldOfView = 90.0f;
};
