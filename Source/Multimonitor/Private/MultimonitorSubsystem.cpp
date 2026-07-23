#include "MultimonitorSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "MultimonitorCaptureActor.h"
#include "MultimonitorLayout.h"
#include "MultimonitorLog.h"
#include "MultimonitorPostProcess.h"
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

void UMultimonitorSubsystem::Tick(float DeltaTime)
{
	for (const TPair<int32, TObjectPtr<UMultimonitorWindow>>& Pair : Windows)
	{
		UMultimonitorWindow* Window = Pair.Value;
		if (!Window)
		{
			continue;
		}

		if (UMultimonitorRenderTargetWidget* Viewer = Cast<UMultimonitorRenderTargetWidget>(Window->GetContentWidget()))
		{
			if (Viewer->WantsAlphaVisualization())
			{
				Viewer->RefreshFromSubsystem();
			}
		}
	}
}

TStatId UMultimonitorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMultimonitorSubsystem, STATGROUP_Tickables);
}

bool UMultimonitorSubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && Windows.Num() > 0;
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

	// Only block the monitor that currently hosts the game/PIE viewport.
	// Do NOT block the OS primary display just because it is primary — that display
	// is often a valid Multimonitor output when PIE lives on another screen.
	if (GEngine && GEngine->GameViewport)
	{
		const TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
		if (GameWindow.IsValid())
		{
			const FVector2D WindowPos = GameWindow->GetPositionInScreen();
			const FVector2D WindowSize = GameWindow->GetSizeInScreen();
			if (WindowSize.X > 1.f && WindowSize.Y > 1.f)
			{
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

	// Also clear external-capture-only slots (no window key).
	TArray<int32> ExternalIndices;
	ExternalCaptures.GetKeys(ExternalIndices);
	for (const int32 MonitorIndex : ExternalIndices)
	{
		DestroyCaptureForMonitor(MonitorIndex);
	}

	Windows.Empty();
	CaptureActors.Empty();
	ExternalCaptures.Empty();
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

	ExternalCaptures.Remove(MonitorIndex);
}

UTextureRenderTarget2D* UMultimonitorSubsystem::GetSlotRenderTarget(int32 MonitorIndex) const
{
	if (const TObjectPtr<AMultimonitorCaptureActor>* Found = CaptureActors.Find(MonitorIndex))
	{
		if (const AMultimonitorCaptureActor* Capture = Found->Get())
		{
			return Capture->GetRenderTarget();
		}
	}

	if (const TObjectPtr<USceneCaptureComponent2D>* Ext = ExternalCaptures.Find(MonitorIndex))
	{
		if (const USceneCaptureComponent2D* Capture = Ext->Get())
		{
			return Capture->TextureTarget;
		}
	}

	return nullptr;
}

bool UMultimonitorSubsystem::IsRenderTargetUsedByOtherSlot(UTextureRenderTarget2D* RenderTarget, int32 ExceptMonitorIndex) const
{
	if (!RenderTarget)
	{
		return false;
	}

	for (const TPair<int32, TObjectPtr<AMultimonitorCaptureActor>>& Pair : CaptureActors)
	{
		if (Pair.Key == ExceptMonitorIndex || !Pair.Value)
		{
			continue;
		}
		if (Pair.Value->GetRenderTarget() == RenderTarget)
		{
			return true;
		}
	}
	return false;
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

	auto AdoptLevelCapture = [this, &Slot](USceneCaptureComponent2D* LevelCapture, const TCHAR* Reason) -> UTextureRenderTarget2D*
	{
		if (!LevelCapture || !LevelCapture->TextureTarget)
		{
			return nullptr;
		}

		MultimonitorPostProcess::ApplyMaterials(LevelCapture, Slot.PostProcessMaterials);
		ExternalCaptures.Add(Slot.MonitorIndex, LevelCapture);

		UE_LOG(LogMultimonitor, Log,
			TEXT("Multimonitor: Monitor %d adopting level SceneCapture '%s' -> RT '%s' (%s)."),
			Slot.MonitorIndex,
			*LevelCapture->GetOwner()->GetName(),
			*LevelCapture->TextureTarget->GetName(),
			Reason);

		return LevelCapture->TextureTarget;
	};

	// Prefer a level SceneCapture on the CameraActor (matches editor Scene Capture workflow).
	if (ViewTarget)
	{
			if (USceneCaptureComponent2D* LevelCapture = ViewTarget->FindComponentByClass<USceneCaptureComponent2D>())
			{
				if (ExistingRT)
				{
					LevelCapture->TextureTarget = ExistingRT;
				}

				if (!LevelCapture->TextureTarget)
				{
					UE_LOG(LogMultimonitor, Warning,
						TEXT("Multimonitor: SceneCapture on '%s' has no Texture Target. Assign one on the component or on the Multimonitor slot."),
						*ViewTarget->GetName());
					return nullptr;
				}

				MultimonitorPostProcess::ConfigureForViewportMatch(LevelCapture);
				return AdoptLevelCapture(LevelCapture, TEXT("CameraActor"));
			}
	}

	// If the slot RT is already written by a level SceneCapture, use that capture.
	// Do NOT spawn a second capture / unique RT — that was showing black while Alpa had content.
	if (ExistingRT)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (USceneCaptureComponent2D* LevelCapture = It->FindComponentByClass<USceneCaptureComponent2D>())
			{
				if (LevelCapture->TextureTarget == ExistingRT)
				{
					return AdoptLevelCapture(LevelCapture, TEXT("shared RT"));
				}
			}
		}
	}

	if (ExistingRT && IsRenderTargetUsedByOtherSlot(ExistingRT, Slot.MonitorIndex))
	{
		UE_LOG(LogMultimonitor, Warning,
			TEXT("Multimonitor: Render target '%s' is already used by another Multimonitor slot. Allocating a unique RT for monitor %d."),
			*ExistingRT->GetName(),
			Slot.MonitorIndex);
		ExistingRT = nullptr;
	}

	FIntPoint Resolution = Slot.CaptureResolution;
	if (Resolution.X <= 0 || Resolution.Y <= 0)
	{
		Resolution = MonitorInfo.NativeResolution;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.Name = *FString::Printf(TEXT("MultimonitorCapture_M%d"), Slot.MonitorIndex);

	AMultimonitorCaptureActor* CaptureActor = World->SpawnActor<AMultimonitorCaptureActor>(
		AMultimonitorCaptureActor::StaticClass(),
		FTransform::Identity,
		SpawnParams);

	if (!CaptureActor)
	{
		SpawnParams.Name = NAME_None;
		CaptureActor = World->SpawnActor<AMultimonitorCaptureActor>(
			AMultimonitorCaptureActor::StaticClass(),
			FTransform::Identity,
			SpawnParams);
	}

	if (!CaptureActor)
	{
		return nullptr;
	}

	CaptureActor->Configure(
		Slot.MonitorIndex,
		ViewTarget,
		ExistingRT,
		Resolution,
		Slot.PostProcessMaterials,
		Slot.bCopyCameraPostProcess,
		Slot.bVisualizeAlpha);
	CaptureActors.Add(Slot.MonitorIndex, CaptureActor);

	UTextureRenderTarget2D* BoundRT = CaptureActor->GetRenderTarget();
	UE_LOG(LogMultimonitor, Log,
		TEXT("Multimonitor: Slot monitor %d bound to RT '%s' (Multimonitor-owned capture)."),
		Slot.MonitorIndex,
		BoundRT ? *BoundRT->GetName() : TEXT("None"));

	return BoundRT;
}

void UMultimonitorSubsystem::ApplyPostProcessToRenderTargetCaptures(
	UWorld* World,
	UTextureRenderTarget2D* RenderTarget,
	const TArray<FMultimonitorPostProcessEntry>& Materials,
	int32 MonitorIndex)
{
	if (!World || !RenderTarget || Materials.Num() == 0)
	{
		return;
	}

	int32 Found = 0;
	USceneCaptureComponent2D* FirstCapture = nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (USceneCaptureComponent2D* LevelCapture = It->FindComponentByClass<USceneCaptureComponent2D>())
		{
			if (LevelCapture->TextureTarget == RenderTarget)
			{
				MultimonitorPostProcess::ApplyMaterials(LevelCapture, Materials);
				++Found;
				if (!FirstCapture)
				{
					FirstCapture = LevelCapture;
				}
			}
		}
	}

	if (FirstCapture)
	{
		ExternalCaptures.Add(MonitorIndex, FirstCapture);
	}

	if (Found == 0)
	{
		UE_LOG(LogMultimonitor, Warning,
			TEXT("Multimonitor: Slot monitor %d has PostProcessMaterials but no level SceneCapture writes RT '%s'. Put PP materials on the SceneCapture itself, or set CameraActor to that capture."),
			MonitorIndex,
			*RenderTarget->GetName());
	}
	else
	{
		UE_LOG(LogMultimonitor, Log,
			TEXT("Multimonitor: Applied PostProcessMaterials to %d SceneCapture(s) writing '%s' (monitor %d)."),
			Found,
			*RenderTarget->GetName(),
			MonitorIndex);
	}
}

bool UMultimonitorSubsystem::SetSlotPostProcessMaterials(int32 MonitorIndex, const TArray<FMultimonitorPostProcessEntry>& Materials)
{
	if (TObjectPtr<AMultimonitorCaptureActor>* Found = CaptureActors.Find(MonitorIndex))
	{
		if (AMultimonitorCaptureActor* Capture = Found->Get())
		{
			Capture->SetPostProcessMaterials(Materials);
			return true;
		}
	}

	if (TObjectPtr<USceneCaptureComponent2D>* Ext = ExternalCaptures.Find(MonitorIndex))
	{
		if (USceneCaptureComponent2D* Capture = Ext->Get())
		{
			MultimonitorPostProcess::ApplyMaterials(Capture, Materials);
			return true;
		}
	}

	UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: No SceneCapture on monitor %d to update post-process."), MonitorIndex);
	return false;
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
			ApplyPostProcessToRenderTargetCaptures(World, RT, Slot.PostProcessMaterials, Slot.MonitorIndex);
			Viewer->SetVisualizeAlpha(Slot.bVisualizeAlpha, Slot.bInvertAlpha);
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
			Viewer->SetVisualizeAlpha(Slot.bVisualizeAlpha, Slot.bInvertAlpha);
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

	// Default: do not cover the monitor that hosts the game/PIE viewport.
	if (!Slot.bAllowPrimaryMonitor && IsPrimaryGameMonitor(Slot.MonitorIndex))
	{
		UE_LOG(LogMultimonitor, Warning,
			TEXT("Multimonitor: Skipping monitor %d — the game/PIE viewport is on that display. Pick another index from Get Monitor Info (your layout has primary OS monitor at index %d). Set Allow Primary Monitor to force."),
			Slot.MonitorIndex,
			GetPrimaryMonitorIndex());
		return false;
	}

	UE_LOG(LogMultimonitor, Log,
		TEXT("Multimonitor: Opening slot on monitor %d (%s) content=%d"),
		Slot.MonitorIndex,
		*MonitorInfo.Name,
		static_cast<int32>(Slot.ContentType));

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
