// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Slate/SlateTextures.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Framework/Application/SlateApplication.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class FStereoViewport : public ISlateViewport
{
public:
void SetTexture(FSlateTexture2DRHIRef* InTexture) { SlateTexture = InTexture; }

virtual FIntPoint GetSize() const override
{
return SlateTexture
? FIntPoint((int32)SlateTexture->GetWidth(), (int32)SlateTexture->GetHeight())
: FIntPoint(1, 1);
}

virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override
{
return SlateTexture ? SlateTexture->GetSlateResource() : nullptr;
}

virtual bool RequiresVsync() const override { return false; }
virtual bool AllowScaling()  const override { return true;  }

private:
FSlateTexture2DRHIRef* SlateTexture = nullptr;
};

struct FStereoWindowSettings
{
int32 MonitorId = 1;
int32 Width     = 3840;
int32 Height    = 1080;
};

class SStereoWindow : public SCompoundWidget
{
public:
SLATE_BEGIN_ARGS(SStereoWindow) {}
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);
virtual ~SStereoWindow();

void Open(const FStereoWindowSettings& InSettings);
void Close();
bool IsOpen() const { return bWindowOpen; }

void UpdateSceneTexture(FTextureRHIRef InSceneRHI, FIntPoint ViewportSize);
void ReleaseResources();
void MoveWindowToMonitor(int32 MonitorId, int32 Width, int32 Height);

private:
static FVector2D GetMonitorOrigin(int32 MonitorId);

TSharedPtr<FStereoViewport>   Viewport;
TSharedPtr<class SViewport>   ViewportWidget;
TSharedPtr<SWindow>           OsWindow;

TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> SlateTexture;
FTextureRHIRef  CopiedSceneTexture;
FIntPoint       CopiedSceneTextureSize = FIntPoint::ZeroValue;
bool            bWindowOpen = false;
};
