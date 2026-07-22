#include "MultimonitorSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "MultimonitorCaptureActor.h"
#include "MultimonitorLayout.h"
#include "MultimonitorLog.h"
#include "MultimonitorRenderTargetWidget.h"
#include "MultimonitorWindow.h"
#include "Widgets/SWindow.h"

void UMultimonitorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMultimonitorSubsystem::Deinitialize()
{
	ClearLayout();
	Super::Deinitialize();
}

int32 UMultimonitorSubsystem::GetMonitorCount() const
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	return DisplayMetrics.MonitorInfo.Num();
}

TArray<FMultimonitorMonitorInfo> UMultimonitorSubsystem::GetMonitorInfo() const
{
	TArray<FMultimonitorMonitorInfo> Result;

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	for (int32 Index = 0; Index < DisplayMetrics.MonitorInfo.Num(); ++Index)
	{
		const FMonitorInfo& Monitor = DisplayMetrics.MonitorInfo[Index];

		FMultimonitorMonitorInfo Info;
		Info.Index = Index;
		Info.Name = Monitor.Name;
		Info.NativeResolution = FIntPoint(Monitor.NativeWidth, Monitor.NativeHeight);
		Info.DisplayRectMin = FIntPoint(Monitor.DisplayRect.Left, Monitor.DisplayRect.Top);
		Info.DisplayRectMax = FIntPoint(Monitor.DisplayRect.Right, Monitor.DisplayRect.Bottom);
		Info.bIsPrimary = Monitor.bIsPrimary;
		Result.Add(Info);
	}

	return Result;
}

int32 UMultimonitorSubsystem::GetPrimaryMonitorIndex() const
{
	const TArray<FMultimonitorMonitorInfo> Monitors = GetMonitorInfo();
	for (const FMultimonitorMonitorInfo& Monitor : Monitors)
	{
		if (Monitor.bIsPrimary)
		{
			return Monitor.Index;
		}
	}
	return Monitors.Num() > 0 ? 0 : INDEX_NONE;
}

bool UMultimonitorSubsystem::TryGetMonitorMetrics(int32 MonitorIndex, FMultimonitorMonitorInfo& OutInfo) const
{
	const TArray<FMultimonitorMonitorInfo> Monitors = GetMonitorInfo();
	if (!Monitors.IsValidIndex(MonitorIndex))
	{
		return false;
	}

	OutInfo = Monitors[MonitorIndex];
	return true;
}

bool UMultimonitorSubsystem::IsPrimaryGameMonitor(int32 MonitorIndex) const
{
	FMultimonitorMonitorInfo MonitorInfo;
	if (!TryGetMonitorMetrics(MonitorIndex, MonitorInfo))
	{
		return false;
	}

	if (MonitorInfo.bIsPrimary || MonitorIndex == GetPrimaryMonitorIndex())
	{
		return true;
	}

	// Prefer detecting which monitor hosts the game viewport window (PIE / standalone).
	if (GEngine && GEngine->GameViewport)
	{
		const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
		if (GameWindow.IsValid())
		{
			const FVector2D WindowPos = GameWindow->GetPositionInScreen();
			const FVector2D WindowSize = GameWindow->GetSizeInScreen();
			const FVector2D WindowCenter = WindowPos + (WindowSize * 0.5f);

			if (WindowCenter.X >= MonitorInfo.DisplayRectMin.X &&
				WindowCenter.X < MonitorInfo.DisplayRectMax.X &&
				WindowCenter.Y >= MonitorInfo.DisplayRectMin.Y &&
				WindowCenter.Y < MonitorInfo.DisplayRectMax.Y)
			{
				return true;
			}
		}
	}

	return false;
}

UWorld* UMultimonitorSubsystem::GetPlayWorld() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetWorld();
	}
	return nullptr;
}

bool UMultimonitorSubsystem::ApplyLayout(UMultimonitorLayout* Layout)
{
	ClearLayout();

	if (!Layout)
	{
		return false;
	}

	ActiveLayout = Layout;

	bool bAnySuccess = false;
	for (const FMultimonitorSlot& Slot : Layout->Slots)
	{
		if (CreateOrUpdateSlot(Slot))
		{
			bAnySuccess = true;
		}
	}

	return bAnySuccess;
}

void UMultimonitorSubsystem::ClearLayout()
{
	TArray<int32> MonitorIndices;
	Windows.GetKeys(MonitorIndices);
	for (const int32 MonitorIndex : MonitorIndices)
	{
		DestroyWindowForMonitor(MonitorIndex);
		DestroyCaptureForMonitor(MonitorIndex);
	}

	Windows.Empty();
	CaptureActors.Empty();
	ActiveLayout = nullptr;
}

bool UMultimonitorSubsystem::SetSlotContent(const FMultimonitorSlot& Slot)
{
	return CreateOrUpdateSlot(Slot);
}

UMultimonitorWindow* UMultimonitorSubsystem::GetWindowForMonitor(int32 MonitorIndex) const
{
	if (const TObjectPtr<UMultimonitorWindow>* Found = Windows.Find(MonitorIndex))
	{
		return Found->Get();
	}
	return nullptr;
}

void UMultimonitorSubsystem::DestroyWindowForMonitor(int32 MonitorIndex)
{
	if (TObjectPtr<UMultimonitorWindow>* Found = Windows.Find(MonitorIndex))
	{
		if (UMultimonitorWindow* Window = Found->Get())
		{
			Window->Close();
		}
		Windows.Remove(MonitorIndex);
	}
}

void UMultimonitorSubsystem::DestroyCaptureForMonitor(int32 MonitorIndex)
{
	if (TObjectPtr<AMultimonitorCaptureActor>* Found = CaptureActors.Find(MonitorIndex))
	{
		if (AMultimonitorCaptureActor* Capture = Found->Get())
		{
			Capture->Destroy();
		}
		CaptureActors.Remove(MonitorIndex);
	}
}

UTextureRenderTarget2D* UMultimonitorSubsystem::EnsureCameraCapture(UWorld* World, const FMultimonitorSlot& Slot, const FMultimonitorMonitorInfo& MonitorInfo)
{
	if (!World)
	{
		return nullptr;
	}

	DestroyCaptureForMonitor(Slot.MonitorIndex);

	AActor* ViewTarget = nullptr;
	if (!Slot.CameraActor.IsNull())
	{
		ViewTarget = Slot.CameraActor.Get();
		if (!ViewTarget)
		{
			ViewTarget = Slot.CameraActor.LoadSynchronous();
		}
	}

	UTextureRenderTarget2D* ExistingRT = nullptr;
	if (!Slot.RenderTarget.IsNull())
	{
		ExistingRT = Slot.RenderTarget.Get();
		if (!ExistingRT)
		{
			ExistingRT = Slot.RenderTarget.LoadSynchronous();
		}
	}

	FIntPoint Resolution = Slot.CaptureResolution;
	if (Resolution.X <= 0 || Resolution.Y <= 0)
	{
		Resolution = MonitorInfo.NativeResolution;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	AMultimonitorCaptureActor* CaptureActor = World->SpawnActor<AMultimonitorCaptureActor>(
		AMultimonitorCaptureActor::StaticClass(),
		FTransform::Identity,
		SpawnParams);

	if (!CaptureActor)
	{
		return nullptr;
	}

	CaptureActor->Configure(ViewTarget, ExistingRT, Resolution);
	CaptureActors.Add(Slot.MonitorIndex, CaptureActor);
	return CaptureActor->GetRenderTarget();
}

UUserWidget* UMultimonitorSubsystem::BuildContentWidget(UWorld* World, const FMultimonitorSlot& Slot, const FMultimonitorMonitorInfo& MonitorInfo)
{
	if (!World)
	{
		return nullptr;
	}

	switch (Slot.ContentType)
	{
	case EMultimonitorContentType::HUD:
	{
		if (!Slot.HUDWidgetClass)
		{
			UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: HUD slot on monitor %d has no widget class."), Slot.MonitorIndex);
			return nullptr;
		}

		return CreateWidget<UUserWidget>(World, Slot.HUDWidgetClass);
	}
	case EMultimonitorContentType::RenderTarget:
	{
		UTextureRenderTarget2D* RT = nullptr;
		if (!Slot.RenderTarget.IsNull())
		{
			RT = Slot.RenderTarget.Get();
			if (!RT)
			{
				RT = Slot.RenderTarget.LoadSynchronous();
			}
		}
		if (!RT)
		{
			UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: RenderTarget slot on monitor %d has no render target."), Slot.MonitorIndex);
			return nullptr;
		}

		UMultimonitorRenderTargetWidget* Viewer = CreateWidget<UMultimonitorRenderTargetWidget>(
			World,
			UMultimonitorRenderTargetWidget::StaticClass());

		if (Viewer)
		{
			Viewer->SetRenderTarget(RT);
		}
		return Viewer;
	}
	case EMultimonitorContentType::Camera:
	{
		UTextureRenderTarget2D* RT = EnsureCameraCapture(World, Slot, MonitorInfo);
		if (!RT)
		{
			UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: Failed to create camera capture for monitor %d."), Slot.MonitorIndex);
			return nullptr;
		}

		UMultimonitorRenderTargetWidget* Viewer = CreateWidget<UMultimonitorRenderTargetWidget>(
			World,
			UMultimonitorRenderTargetWidget::StaticClass());

		if (Viewer)
		{
			Viewer->SetRenderTarget(RT);
		}
		return Viewer;
	}
	default:
		return nullptr;
	}
}

bool UMultimonitorSubsystem::CreateOrUpdateSlot(const FMultimonitorSlot& Slot)
{
	if (Slot.ContentType == EMultimonitorContentType::None)
	{
		DestroyWindowForMonitor(Slot.MonitorIndex);
		DestroyCaptureForMonitor(Slot.MonitorIndex);
		return true;
	}

	FMultimonitorMonitorInfo MonitorInfo;
	if (!TryGetMonitorMetrics(Slot.MonitorIndex, MonitorInfo))
	{
		UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: Invalid monitor index %d."), Slot.MonitorIndex);
		return false;
	}

	// Default: do not take over the monitor hosting the primary game viewport.
	if (!Slot.bAllowPrimaryMonitor && IsPrimaryGameMonitor(Slot.MonitorIndex))
	{
		UE_LOG(LogMultimonitor, Warning,
			TEXT("Multimonitor: Skipping monitor %d (primary/game viewport). Use a secondary Monitor Index (often 1). Set bAllowPrimaryMonitor only if you intentionally want to cover that display."),
			Slot.MonitorIndex);
		return false;
	}

	UWorld* World = GetPlayWorld();
	if (!World)
	{
		UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: No play world available."));
		return false;
	}

	if (Slot.ContentType != EMultimonitorContentType::Camera)
	{
		DestroyCaptureForMonitor(Slot.MonitorIndex);
	}

	UUserWidget* ContentWidget = BuildContentWidget(World, Slot, MonitorInfo);
	if (!ContentWidget)
	{
		return false;
	}

	DestroyWindowForMonitor(Slot.MonitorIndex);

	UMultimonitorWindow* Window = NewObject<UMultimonitorWindow>(this);
	const FVector2D ScreenPosition(MonitorInfo.DisplayRectMin.X, MonitorInfo.DisplayRectMin.Y);
	const FVector2D ClientSize(
		MonitorInfo.NativeResolution.X > 0 ? MonitorInfo.NativeResolution.X : (MonitorInfo.DisplayRectMax.X - MonitorInfo.DisplayRectMin.X),
		MonitorInfo.NativeResolution.Y > 0 ? MonitorInfo.NativeResolution.Y : (MonitorInfo.DisplayRectMax.Y - MonitorInfo.DisplayRectMin.Y));

	const FText Title = FText::FromString(FString::Printf(TEXT("Multimonitor %d"), Slot.MonitorIndex));
	if (!Window->Open(World, Slot.MonitorIndex, ScreenPosition, ClientSize, Slot.bFullscreen, Title))
	{
		UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: Failed to open window for monitor %d."), Slot.MonitorIndex);
		DestroyCaptureForMonitor(Slot.MonitorIndex);
		return false;
	}

	Window->SetContentWidget(ContentWidget);
	Windows.Add(Slot.MonitorIndex, Window);
	return true;
}
