// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStereoWindow.h"
#include "SStereoViewportClient.h"
#include "SlateOptMacros.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "StereoRendering.h"

// ---------------------------------------------------------------------------
// Construct
// ---------------------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SStereoWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SAssignNew(ViewportWidget, SViewport)
				.RenderDirectlyToWindow(false)
				.EnableGammaCorrection(false)
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
SStereoWindow::~SStereoWindow()
{
	Close();
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
	Data.TargetIndex = MonitorId;
	Data.CurrentIndex = 0;
	Data.bFound = false;
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
void SStereoWindow::Open(UWorld* World, AActor* Owner, const FStereoWindowSettings& InSettings)
{
	if (bWindowOpen) return;

	if (!World || !GEngine || !FSlateApplication::IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SStereoWindow] Open: preconditions not met (World=%s GEngine=%s Slate=%s)."),
			World ? TEXT("OK") : TEXT("null"),
			GEngine ? TEXT("OK") : TEXT("null"),
			FSlateApplication::IsInitialized() ? TEXT("OK") : TEXT("no"));
		return;
	}

	// 1. Create the viewport client.
	// AddToRoot prevents GC — the client is not owned by a GameInstance.
	ViewportClient = NewObject<USStereoViewportClient>(GEngine, USStereoViewportClient::StaticClass());
	ViewportClient->AddToRoot();
	ViewportClient->TargetWorld = World;

	// 2. Wrap the SViewport widget in an FSceneViewport.
	SceneViewport = MakeShared<FSceneViewport>(ViewportClient, ViewportWidget);
	ViewportClient->Viewport = SceneViewport.Get();
	ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());

	// DisplayGamma is synchronized from the main game viewport each frame inside
	// USStereoViewportClient::Draw(), so no static override is needed here.

	// 3. Create the OS window and embed this widget as its content.
	OsWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Stereo Output")))
		.ClientSize(FVector2D((float)InSettings.Width, (float)InSettings.Height))

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

	ViewportClient->TargetWindow = OsWindow;

	// 4. Enable stereo on the SViewport widget (does not touch GEngine->StereoRenderingDevice).
	ViewportClient->InitStereoRendering(ViewportWidget);

	// 5. Position the window on the requested monitor.
	MoveWindowToMonitor(InSettings.MonitorId, InSettings.Width, InSettings.Height);

	UE_LOG(LogTemp, Log, TEXT("[SStereoWindow] Opened on monitor %d (%dx%d)."),
		InSettings.MonitorId, InSettings.Width, InSettings.Height);
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------
void SStereoWindow::Close()
{
	if (!bWindowOpen) return;

	SceneViewport.Reset();

	if (OsWindow.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			OsWindow->RequestDestroyWindow();
		}
		OsWindow.Reset();
	}

	if (ViewportClient)
	{
		ViewportClient->RemoveFromRoot();
		ViewportClient = nullptr;
	}

	bWindowOpen = false;
	UE_LOG(LogTemp, Log, TEXT("[SStereoWindow] Closed."));
}

// ---------------------------------------------------------------------------
// Tick — call once per frame to trigger the scene render
// ---------------------------------------------------------------------------
void SStereoWindow::Tick()
{
	if (!bWindowOpen || !SceneViewport.IsValid() || !OsWindow.IsValid())
	{
		return;
	}

	if (!OsWindow->GetNativeWindow().IsValid())
	{
		return;
	}

	// Temporarily clear GEngine->StereoRenderingDevice during the stereo window draw.
	// FSceneViewport::Draw() calls StereoRenderingDevice->NeedReAllocateViewportRenderTarget()
	// on every viewport it draws. With OpenXR active, the HMD device forces our stereo window
	// render target to its preferred resolution (e.g. 1168x560) each frame, conflicting with
	// the configured stereo output resolution (e.g. 2560x1080) and causing repeated
	// "Resizing VR buffer" log spam. Hiding the device during this draw prevents HMD involvement.
	// The main game viewport draw (Super::Draw) restores normal HMD behavior immediately after.
	TSharedPtr<IStereoRendering, ESPMode::ThreadSafe> SavedStereoDevice;
	if (GEngine)
	{
		SavedStereoDevice = GEngine->StereoRenderingDevice;
		GEngine->StereoRenderingDevice = nullptr;
	}

	SceneViewport->Draw(true);

	if (GEngine)
	{
		GEngine->StereoRenderingDevice = SavedStereoDevice;
	}
}

// ---------------------------------------------------------------------------
// SetCameraPose
// ---------------------------------------------------------------------------
void SStereoWindow::SetCameraPose(const FVector& Location, const FRotator& Rotation)
{
	if (!ViewportClient) return;
	ViewportClient->CameraLocation = Location;
	ViewportClient->CameraRotation = Rotation;
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

	UE_LOG(LogTemp, Log, TEXT("[SStereoWindow] Monitor=%d Origin=(%.0f,%.0f) Size=%dx%d"),
		MonitorId, Origin.X, Origin.Y, Width, Height);
}
