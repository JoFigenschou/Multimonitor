#pragma once

#include "CoreMinimal.h"
#include "MultimonitorTypes.generated.h"

class AActor;
class UTextureRenderTarget2D;
class UUserWidget;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EMultimonitorContentType : uint8
{
	None UMETA(DisplayName = "None"),
	Camera UMETA(DisplayName = "Camera"),
	RenderTarget UMETA(DisplayName = "Render Target"),
	HUD UMETA(DisplayName = "HUD")
};

USTRUCT(BlueprintType)
struct MULTIMONITOR_API FMultimonitorMonitorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Multimonitor")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Multimonitor")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Multimonitor")
	FIntPoint NativeResolution = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Multimonitor")
	FIntPoint DisplayRectMin = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Multimonitor")
	FIntPoint DisplayRectMax = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Multimonitor")
	bool bIsPrimary = false;
};

USTRUCT(BlueprintType)
struct MULTIMONITOR_API FMultimonitorPostProcessEntry
{
	GENERATED_BODY()

	/** Post-process domain material (not a UI material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct MULTIMONITOR_API FMultimonitorSlot
{
	GENERATED_BODY()

	/** Zero-based monitor index from GetMonitorInfo / display metrics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor")
	int32 MonitorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor")
	EMultimonitorContentType ContentType = EMultimonitorContentType::None;

	/** Camera or actor whose transform drives a SceneCapture for Camera content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (EditCondition = "ContentType == EMultimonitorContentType::Camera", EditConditionHides))
	TSoftObjectPtr<AActor> CameraActor;

	/** Existing render target to show, or optional target for Camera captures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (EditCondition = "ContentType == EMultimonitorContentType::Camera || ContentType == EMultimonitorContentType::RenderTarget", EditConditionHides))
	TSoftObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** Widget class spawned as full-window content for HUD slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (EditCondition = "ContentType == EMultimonitorContentType::HUD", EditConditionHides))
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/**
	 * Extra post-process materials applied on Camera captures (post-process domain).
	 * Combined with materials already on the camera when bCopyCameraPostProcess is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (EditCondition = "ContentType == EMultimonitorContentType::Camera", EditConditionHides))
	TArray<FMultimonitorPostProcessEntry> PostProcessMaterials;

	/**
	 * When true (default), copies Post Process Settings / blendables from the Camera/CineCamera
	 * onto the SceneCapture so materials on the camera affect Multimonitor output.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (EditCondition = "ContentType == EMultimonitorContentType::Camera", EditConditionHides))
	bool bCopyCameraPostProcess = true;

	/** When true, the secondary window covers the entire monitor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor")
	bool bFullscreen = true;

	/**
	 * If false (default), slots targeting the monitor that hosts the game/PIE viewport are refused.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor")
	bool bAllowPrimaryMonitor = false;

	/**
	 * Capture resolution for Camera slots when no RenderTarget is supplied.
	 * (0,0) means match the target monitor native resolution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor", meta = (EditCondition = "ContentType == EMultimonitorContentType::Camera", EditConditionHides))
	FIntPoint CaptureResolution = FIntPoint::ZeroValue;
};
