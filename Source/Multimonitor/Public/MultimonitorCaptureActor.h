#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MultimonitorTypes.h"
#include "MultimonitorCaptureActor.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UCameraComponent;

/**
 * Transient helper that captures a view for a Multimonitor camera slot.
 * Each instance owns (or uniquely binds) one render target — never share RTs across slots.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class MULTIMONITOR_API AMultimonitorCaptureActor : public AActor
{
	GENERATED_BODY()

public:
	AMultimonitorCaptureActor();

	void Configure(
		int32 InMonitorIndex,
		AActor* InViewTarget,
		UTextureRenderTarget2D* InRenderTarget,
		const FIntPoint& FallbackResolution,
		const TArray<FMultimonitorPostProcessEntry>& InPostProcessMaterials,
		bool bInCopyCameraPostProcess);

	void SetViewTarget(AActor* InViewTarget);
	void SetPostProcessMaterials(const TArray<FMultimonitorPostProcessEntry>& InPostProcessMaterials);

	USceneCaptureComponent2D* GetCaptureComponent() const { return CaptureComponent; }
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
	int32 GetMonitorIndex() const { return MonitorIndex; }

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Multimonitor")
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ViewTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> OwnedRenderTarget;

	UPROPERTY(Transient)
	TArray<FMultimonitorPostProcessEntry> ExtraPostProcessMaterials;

	int32 MonitorIndex = INDEX_NONE;
	bool bCopyCameraPostProcess = true;

	void SyncTransformToViewTarget();
	void ApplyPostProcess();
	void TrySyncExposureFromPlayerView(UCameraComponent* CameraComp);
	UTextureRenderTarget2D* CreateOwnedRenderTarget(const FIntPoint& Resolution);
};
