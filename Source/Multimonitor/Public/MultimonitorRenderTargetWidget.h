#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MultimonitorRenderTargetWidget.generated.h"

class UImage;
class UTexture2D;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Full-bleed widget that displays a render target (optionally via a UI material). */
UCLASS(Blueprintable)
class MULTIMONITOR_API UMultimonitorRenderTargetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	void SetDisplayMaterial(UMaterialInterface* InMaterial, FName TextureParameterName = TEXT("RenderTarget"));

	/** Copy source RT.A into a temp texture (R/G/B = A) and display that. */
	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	void SetVisualizeAlpha(bool bEnabled, bool bInvert = true);

	/** Called from MultimonitorSubsystem tick (external SWindow widgets do not NativeTick). */
	void RefreshFromSubsystem();

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	bool WantsAlphaVisualization() const { return bVisualizeAlpha; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Multimonitor")
	TObjectPtr<UImage> DisplayImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** Temp texture with alpha written into RGB for Visualize Alpha. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> AlphaVizTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DisplayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DisplayMID;

	FName TextureParamName = TEXT("RenderTarget");
	bool bVisualizeAlpha = false;
	bool bInvertAlpha = true;
	TArray<FColor> AlphaPixelScratch;

	void ApplyToImage();
	void EnsureDisplayImage();
	void UpdateAlphaVisualization();
};
