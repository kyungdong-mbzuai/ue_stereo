// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SimStereoRendering.h"
#include "SimGameInstance.generated.h"

/**
 * Custom GameInstance that enables stereo rendering when the game is initialized.
 */
UCLASS()
class UE_STEREO_API USimGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

private:
	void EnableStereo();

	TSharedPtr<FSimStereoRendering, ESPMode::ThreadSafe> SimStereoRenderingDevice;
};
