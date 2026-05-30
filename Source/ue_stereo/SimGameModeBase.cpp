// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimGameModeBase.h"
#include "SimPlayerController.h"

ASimGameModeBase::ASimGameModeBase()
{
	PlayerControllerClass = ASimPlayerController::StaticClass();
}
