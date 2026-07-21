#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MultimonitorRenderTargetWidget.generated.h"

class UImage;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Full-bleed widget that displays a render target.
 * Optionally applies a UI-domain material that samples the RT.
 */
UCLASS(Blueprintable)
class MULTIMONITOR_API UMultimonitorRenderTargetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	void SetDisplayMaterial(UMaterialInterface* InMaterial, FName TextureParameterName = TEXT("RenderTarget"));

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Multimonitor")
	TObjectPtr<UImage> DisplayImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DisplayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DisplayMID;

	FName TextureParamName = TEXT("RenderTarget");

	void ApplyToImage();
	void EnsureDisplayImage();
};
