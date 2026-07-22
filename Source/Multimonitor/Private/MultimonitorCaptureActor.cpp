#include "MultimonitorCaptureActor.h"

#include "CineCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"

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
}

void AMultimonitorCaptureActor::Configure(AActor* InViewTarget, UTextureRenderTarget2D* InRenderTarget, const FIntPoint& FallbackResolution)
{
	ViewTarget = InViewTarget;

	if (InRenderTarget)
	{
		RenderTarget = InRenderTarget;
		OwnedRenderTarget = nullptr;
	}
	else
	{
		const int32 SizeX = FallbackResolution.X > 0 ? FallbackResolution.X : 1920;
		const int32 SizeY = FallbackResolution.Y > 0 ? FallbackResolution.Y : 1080;

		OwnedRenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
		OwnedRenderTarget->ClearColor = FLinearColor::Black;
		OwnedRenderTarget->bAutoGenerateMips = false;
		OwnedRenderTarget->RenderTargetFormat = RTF_RGBA8;
		OwnedRenderTarget->InitCustomFormat(SizeX, SizeY, PF_B8G8R8A8, false);
		OwnedRenderTarget->UpdateResourceImmediate(true);
		RenderTarget = OwnedRenderTarget;
	}

	CaptureComponent->TextureTarget = RenderTarget;
	SyncTransformToViewTarget();
	CaptureComponent->CaptureScene();
}

void AMultimonitorCaptureActor::SetViewTarget(AActor* InViewTarget)
{
	ViewTarget = InViewTarget;
	SyncTransformToViewTarget();
}

void AMultimonitorCaptureActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncTransformToViewTarget();
}

void AMultimonitorCaptureActor::SyncTransformToViewTarget()
{
	if (!ViewTarget || !CaptureComponent)
	{
		return;
	}

	if (UCineCameraComponent* CineCamera = ViewTarget->FindComponentByClass<UCineCameraComponent>())
	{
		CaptureComponent->SetWorldTransform(CineCamera->GetComponentTransform());
		CaptureComponent->FOVAngle = CineCamera->GetHorizontalFieldOfView();
		return;
	}

	if (UCameraComponent* CameraComp = ViewTarget->FindComponentByClass<UCameraComponent>())
	{
		CaptureComponent->SetWorldTransform(CameraComp->GetComponentTransform());
		CaptureComponent->FOVAngle = CameraComp->FieldOfView;
		return;
	}

	CaptureComponent->SetWorldTransform(ViewTarget->GetActorTransform());
}
