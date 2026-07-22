#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MultimonitorTypes.h"
#include "MultimonitorCaptureActor.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * Transient helper that captures a view for a Multimonitor camera slot.
 * Syncs transform and post-process from a target camera when assigned.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class MULTIMONITOR_API AMultimonitorCaptureActor : public AActor
{
	GENERATED_BODY()

public:
	AMultimonitorCaptureActor();

	void Configure(
		AActor* InViewTarget,
		UTextureRenderTarget2D* InRenderTarget,
		const FIntPoint& FallbackResolution,
		const TArray<FMultimonitorPostProcessEntry>& InPostProcessMaterials,
		bool bInCopyCameraPostProcess);

	void SetViewTarget(AActor* InViewTarget);
	void SetPostProcessMaterials(const TArray<FMultimonitorPostProcessEntry>& InPostProcessMaterials);

	USceneCaptureComponent2D* GetCaptureComponent() const { return CaptureComponent; }
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

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

	bool bCopyCameraPostProcess = true;

	void SyncTransformToViewTarget();
	void ApplyPostProcess();
};
