// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStereoWindow.h"
#include "SlateOptMacros.h"
#include "Widgets/SViewport.h"
#include "RenderResource.h"
#include "RenderingThread.h"

// ---------------------------------------------------------------------------
// Construct
// ---------------------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SStereoWindow::Construct(const FArguments& InArgs)
{
	Viewport = MakeShared<FStereoViewport>();

	ChildSlot
	[
		SAssignNew(ViewportWidget, SViewport)
		.EnableGammaCorrection(false)
		.RenderDirectlyToWindow(false)
		.IsEnabled(false)
	];

	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
SStereoWindow::~SStereoWindow()
{
	ReleaseResources();
}

// ---------------------------------------------------------------------------
// Windows monitor enumeration helper
// ---------------------------------------------------------------------------
#if PLATFORM_WINDOWS
struct FMonitorFindData
{
	int32    TargetIndex;
	int32    CurrentIndex;
	FIntRect FoundRect;
	bool     bFound;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT lprc, LPARAM lParam)
{
	auto* D = reinterpret_cast<FMonitorFindData*>(lParam);
	if (!D) return 1;
	if (D->CurrentIndex == D->TargetIndex)
	{
		D->FoundRect = FIntRect(lprc->left, lprc->top, lprc->right, lprc->bottom);
		D->bFound = true;
		return 0;
	}
	D->CurrentIndex++;
	return 1;
}
#endif

FVector2D SStereoWindow::GetMonitorOrigin(int32 MonitorId)
{
	FVector2D Origin(0.0f, 0.0f);
#if PLATFORM_WINDOWS
	FMonitorFindData Data;
	Data.TargetIndex  = MonitorId;
	Data.CurrentIndex = 0;
	Data.bFound       = false;
	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&Data));
	if (Data.bFound)
	{
		Origin = FVector2D((float)Data.FoundRect.Min.X, (float)Data.FoundRect.Min.Y);
	}
#endif
	return Origin;
}

// ---------------------------------------------------------------------------
// Open
// ---------------------------------------------------------------------------
void SStereoWindow::Open(const FStereoWindowSettings& InSettings)
{
	if (bWindowOpen) return;

	OsWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Stereo Output")))
		.SizingRule(ESizingRule::FixedSize)
		.bDragAnywhere(false)
		.HasCloseButton(false)
		.IsTopmostWindow(false)
		.CreateTitleBar(false)
		.ShouldPreserveAspectRatio(false)
		[
			SharedThis(this)
		];

	FSlateApplication::Get().AddWindow(OsWindow.ToSharedRef(), true);
	bWindowOpen = true;

	MoveWindowToMonitor(InSettings.MonitorId, InSettings.Width, InSettings.Height);
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------
void SStereoWindow::Close()
{
	if (!bWindowOpen) return;

	// Detach texture from viewport before window destruction.
	if (Viewport.IsValid())
	{
		Viewport->SetTexture(nullptr);
	}

	if (OsWindow.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			OsWindow->RequestDestroyWindow();
		}
		OsWindow.Reset();
	}

	bWindowOpen = false;
	UE_LOG(LogTemp, Log, TEXT("SStereoWindow: Closed"));
}

// ---------------------------------------------------------------------------
// UpdateSceneTexture
// ---------------------------------------------------------------------------
void SStereoWindow::UpdateSceneTexture(FTextureRHIRef InSceneRHI, FIntPoint ViewportSize)
{
	if (!bWindowOpen || !InSceneRHI.IsValid()) return;

	const EPixelFormat SceneFormat = InSceneRHI->GetFormat();

	// Reallocate when size OR format changes (format mismatch causes silent CopyTexture failure).
	const bool bNeedRealloc = !CopiedSceneTexture.IsValid()
		|| CopiedSceneTextureSize != ViewportSize
		|| CopiedSceneTexture->GetFormat() != SceneFormat;

	if (bNeedRealloc)
	{
		// Detach from viewport before releasing old resources.
		if (Viewport.IsValid())
		{
			Viewport->SetTexture(nullptr);
		}

		if (SlateTexture.IsValid())
		{
			BeginReleaseResource(SlateTexture.Get());
			FlushRenderingCommands();
			SlateTexture.Reset();
		}

		CopiedSceneTexture.SafeRelease();
		CopiedSceneTextureSize = ViewportSize;

		// CopiedSceneTexture must have the SAME format as InSceneRHI so CopyTexture
		// does not fail silently. RenderTargetable so it can be a copy destination.
		ENQUEUE_RENDER_COMMAND(SStereoAllocCopied)(
			[this, ViewportSize, SceneFormat](FRHICommandListImmediate& RHICmdList)
			{
				const FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create2D(TEXT("SStereoCopied"),
						ViewportSize.X, ViewportSize.Y, SceneFormat)
					.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource);
				CopiedSceneTexture = RHICmdList.CreateTexture(Desc);
			}
		);
		FlushRenderingCommands();

		// FSlateTexture2DRHIRef(FTextureRHIRef, w, h) wraps an existing RHI texture.
		// ShaderResource is set in TSlateTexture constructor; InitRHI is a no-op.
		SlateTexture = MakeShareable(
			new FSlateTexture2DRHIRef(CopiedSceneTexture,
				(uint32)ViewportSize.X, (uint32)ViewportSize.Y)
		);
		BeginInitResource(SlateTexture.Get());
		FlushRenderingCommands();

		if (Viewport.IsValid())
		{
			Viewport->SetTexture(SlateTexture.Get());
		}

		UE_LOG(LogTemp, Warning, TEXT("SStereoWindow: (Re)allocated %dx%d fmt=%d"),
			ViewportSize.X, ViewportSize.Y, (int32)SceneFormat);
	}

	// Copy the live scene RHI into CopiedSceneTexture (= SlateTexture's backing resource).
	ENQUEUE_RENDER_COMMAND(SStereoWindowCopy)(
		[Src = InSceneRHI, Dst = CopiedSceneTexture](FRHICommandListImmediate& RHICmdList)
		{
			if (Src.IsValid() && Dst.IsValid())
			{
				RHICmdList.CopyTexture(Src, Dst, FRHICopyTextureInfo());
			}
		}
	);
}

// ---------------------------------------------------------------------------
// ReleaseResources
// ---------------------------------------------------------------------------
void SStereoWindow::ReleaseResources()
{
	Close();

	if (SlateTexture.IsValid())
	{
		if (Viewport.IsValid())
		{
			Viewport->SetTexture(nullptr);
		}
		BeginReleaseResource(SlateTexture.Get());
		FlushRenderingCommands();
		SlateTexture.Reset();
	}

	CopiedSceneTexture.SafeRelease();
}

// ---------------------------------------------------------------------------
// MoveWindowToMonitor
// ---------------------------------------------------------------------------
void SStereoWindow::MoveWindowToMonitor(int32 MonitorId, int32 Width, int32 Height)
{
	if (!OsWindow.IsValid()) return;

	const FVector2D Origin = GetMonitorOrigin(MonitorId);
	OsWindow->Resize(FVector2D((float)Width, (float)Height));
	OsWindow->MoveWindowTo(Origin);

	UE_LOG(LogTemp, Warning, TEXT("SStereoWindow: Monitor=%d Origin=(%.0f,%.0f) Size=%dx%d"),
		MonitorId, Origin.X, Origin.Y, Width, Height);
}
