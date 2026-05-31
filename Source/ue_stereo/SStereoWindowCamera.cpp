// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStereoWindowCamera.h"

ASStereoWindowCamera::ASStereoWindowCamera()
{
	PrimaryActorTick.bCanEverTick = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetFieldOfView(FieldOfView);
	SetRootComponent(CameraComponent);
}
