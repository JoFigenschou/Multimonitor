#include "MultimonitorCaptureActor.h"

#include "CineCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "MultimonitorLog.h"
#include "MultimonitorPostProcess.h"

AMultimonitorCaptureActor::AMultimonitorCaptureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureComponent"));
	SetRootComponent(CaptureComponent);

	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = true;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	CaptureComponent->PostProcessBlendWeight = 1.0f;
	MultimonitorPostProcess::ConfigureForViewportMatch(CaptureComponent);
}

UTextureRenderTarget2D* AMultimonitorCaptureActor::CreateOwnedRenderTarget(const FIntPoint& Resolution)
{
	const int32 SizeX = Resolution.X > 0 ? Resolution.X : 1920;
	const int32 SizeY = Resolution.Y > 0 ? Resolution.Y : 1080;

	const FName RTName = *FString::Printf(TEXT("MultimonitorRT_M%d"), MonitorIndex);
	UTextureRenderTarget2D* NewRT = NewObject<UTextureRenderTarget2D>(this, RTName, RF_Transient);
	NewRT->ClearColor = FLinearColor::Black;
	NewRT->bAutoGenerateMips = false;
	// 16-bit keeps tonemapped FinalToneCurveHDR with less banding than RGBA8.
	NewRT->RenderTargetFormat = RTF_RGBA16f;
	NewRT->InitAutoFormat(SizeX, SizeY);
	NewRT->UpdateResourceImmediate(true);
	return NewRT;
}

void AMultimonitorCaptureActor::Configure(
	int32 InMonitorIndex,
	AActor* InViewTarget,
	UTextureRenderTarget2D* InRenderTarget,
	const FIntPoint& FallbackResolution,
	const TArray<FMultimonitorPostProcessEntry>& InPostProcessMaterials,
	bool bInCopyCameraPostProcess)
{
	MonitorIndex = InMonitorIndex;
	ViewTarget = InViewTarget;
	ExtraPostProcessMaterials = InPostProcessMaterials;
	bCopyCameraPostProcess = bInCopyCameraPostProcess;

	OwnedRenderTarget = nullptr;

	if (InRenderTarget)
	{
		RenderTarget = InRenderTarget;
	}
	else
	{
		OwnedRenderTarget = CreateOwnedRenderTarget(FallbackResolution);
		RenderTarget = OwnedRenderTarget;
	}

	CaptureComponent->TextureTarget = RenderTarget;
	MultimonitorPostProcess::ConfigureForViewportMatch(CaptureComponent);
	CaptureComponent->SetCaptureSortPriority(1000 + MonitorIndex);

	UE_LOG(LogMultimonitor, Log,
		TEXT("Multimonitor: Capture for monitor %d -> RT '%s' (%dx%d) viewportMatch=1 ptr=%p"),
		MonitorIndex,
		RenderTarget ? *RenderTarget->GetName() : TEXT("None"),
		RenderTarget ? RenderTarget->SizeX : 0,
		RenderTarget ? RenderTarget->SizeY : 0,
		RenderTarget.Get());

	SyncTransformToViewTarget();
	ApplyPostProcess();
}

void AMultimonitorCaptureActor::SetViewTarget(AActor* InViewTarget)
{
	ViewTarget = InViewTarget;
	SyncTransformToViewTarget();
	ApplyPostProcess();
}

void AMultimonitorCaptureActor::SetPostProcessMaterials(const TArray<FMultimonitorPostProcessEntry>& InPostProcessMaterials)
{
	ExtraPostProcessMaterials = InPostProcessMaterials;
	ApplyPostProcess();
}

void AMultimonitorCaptureActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncTransformToViewTarget();

	if (bCopyCameraPostProcess && CaptureComponent && ViewTarget)
	{
		if (UCameraComponent* CameraComp = ViewTarget->FindComponentByClass<UCameraComponent>())
		{
			MultimonitorPostProcess::SyncExposureFromCamera(CaptureComponent, CameraComp);
			TrySyncExposureFromPlayerView(CameraComp);
		}
	}
}

void AMultimonitorCaptureActor::TrySyncExposureFromPlayerView(UCameraComponent* CameraComp)
{
	UWorld* World = GetWorld();
	if (!World || !CaptureComponent || !ViewTarget)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const AActor* PlayerViewTarget = PC->GetViewTarget();
	if (PlayerViewTarget != ViewTarget && PlayerViewTarget != ViewTarget->GetOwner())
	{
		// Secondary camera — keep its own exposure, don't steal the player view's.
		return;
	}

	const FMinimalViewInfo& Cache = PC->PlayerCameraManager->GetCameraCacheView();
	FPostProcessSettings& Dst = CaptureComponent->PostProcessSettings;

	Dst.bOverride_AutoExposureBias = true;
	Dst.AutoExposureBias = Cache.PostProcessSettings.AutoExposureBias;

	if (Cache.PostProcessSettings.bOverride_AutoExposureMethod)
	{
		Dst.bOverride_AutoExposureMethod = true;
		Dst.AutoExposureMethod = Cache.PostProcessSettings.AutoExposureMethod;
	}

	if (CameraComp)
	{
		CaptureComponent->PostProcessBlendWeight = FMath::Clamp(CameraComp->PostProcessBlendWeight, 0.0f, 1.0f);
		if (CaptureComponent->PostProcessBlendWeight <= KINDA_SMALL_NUMBER)
		{
			CaptureComponent->PostProcessBlendWeight = 1.0f;
		}
	}
}

void AMultimonitorCaptureActor::SyncTransformToViewTarget()
{
	if (!ViewTarget || !CaptureComponent)
	{
		return;
	}

	if (UCineCameraComponent* CineCamera = ViewTarget->FindComponentByClass<UCineCameraComponent>())
	{
		FMinimalViewInfo ViewInfo;
		CineCamera->GetCameraView(0.0f, ViewInfo);
		CaptureComponent->SetWorldLocationAndRotation(ViewInfo.Location, ViewInfo.Rotation);
		CaptureComponent->FOVAngle = ViewInfo.FOV;
		CaptureComponent->bUseCustomProjectionMatrix = false;
		return;
	}

	if (UCameraComponent* CameraComp = ViewTarget->FindComponentByClass<UCameraComponent>())
	{
		FMinimalViewInfo ViewInfo;
		CameraComp->GetCameraView(0.0f, ViewInfo);
		CaptureComponent->SetWorldLocationAndRotation(ViewInfo.Location, ViewInfo.Rotation);
		CaptureComponent->FOVAngle = ViewInfo.FOV;
		return;
	}

	CaptureComponent->SetWorldTransform(ViewTarget->GetActorTransform());
}

void AMultimonitorCaptureActor::ApplyPostProcess()
{
	if (!CaptureComponent)
	{
		return;
	}

	MultimonitorPostProcess::ConfigureForViewportMatch(CaptureComponent);

	FPostProcessSettings Settings;

	if (bCopyCameraPostProcess && ViewTarget)
	{
		if (UCameraComponent* CameraComp = ViewTarget->FindComponentByClass<UCameraComponent>())
		{
			MultimonitorPostProcess::CopyCameraLookToSettings(CameraComp, Settings);
			CaptureComponent->PostProcessBlendWeight = FMath::Clamp(CameraComp->PostProcessBlendWeight, 0.0f, 1.0f);
			if (CaptureComponent->PostProcessBlendWeight <= KINDA_SMALL_NUMBER)
			{
				CaptureComponent->PostProcessBlendWeight = 1.0f;
			}
		}
	}

	CaptureComponent->PostProcessSettings = Settings;
	MultimonitorPostProcess::ApplyMaterials(CaptureComponent, ExtraPostProcessMaterials);
}
