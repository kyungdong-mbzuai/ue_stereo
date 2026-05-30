// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SimGameModeBase.generated.h"

/**
 * GameMode that sets ASimPlayerController as the default PlayerController class.
 */
UCLASS()
class UE_STEREO_API ASimGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASimGameModeBase();
};
