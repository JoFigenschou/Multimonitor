#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MultimonitorTypes.h"
#include "MultimonitorSubsystem.generated.h"

class UMultimonitorLayout;
class UMultimonitorWindow;
class AMultimonitorCaptureActor;
class UTextureRenderTarget2D;
class UUserWidget;
class USceneCaptureComponent2D;

UCLASS()
class MULTIMONITOR_API UMultimonitorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	int32 GetMonitorCount() const;

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	TArray<FMultimonitorMonitorInfo> GetMonitorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	bool ApplyLayout(UMultimonitorLayout* Layout);

	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	void ClearLayout();

	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	bool SetSlotContent(const FMultimonitorSlot& Slot);

	/** Live-update post-process materials on an existing Camera/RenderTarget SceneCapture slot. */
	UFUNCTION(BlueprintCallable, Category = "Multimonitor")
	bool SetSlotPostProcessMaterials(int32 MonitorIndex, const TArray<FMultimonitorPostProcessEntry>& Materials);

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	UMultimonitorWindow* GetWindowForMonitor(int32 MonitorIndex) const;

	/** Render target currently driving a Camera slot (owned or user-supplied). */
	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	UTextureRenderTarget2D* GetSlotRenderTarget(int32 MonitorIndex) const;

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	int32 GetPrimaryMonitorIndex() const;

protected:
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMultimonitorWindow>> Windows;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AMultimonitorCaptureActor>> CaptureActors;

	/** Level SceneCaptures Multimonitor is driving for PP (not owned / not destroyed). */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USceneCaptureComponent2D>> ExternalCaptures;

	UPROPERTY(Transient)
	TObjectPtr<UMultimonitorLayout> ActiveLayout;

	bool TryGetMonitorMetrics(int32 MonitorIndex, FMultimonitorMonitorInfo& OutInfo) const;
	bool IsPrimaryGameMonitor(int32 MonitorIndex) const;
	bool IsRenderTargetUsedByOtherSlot(UTextureRenderTarget2D* RenderTarget, int32 ExceptMonitorIndex) const;
	bool CreateOrUpdateSlot(const FMultimonitorSlot& Slot);
	UUserWidget* BuildContentWidget(UWorld* World, const FMultimonitorSlot& Slot, const FMultimonitorMonitorInfo& MonitorInfo);
	UTextureRenderTarget2D* EnsureCameraCapture(UWorld* World, const FMultimonitorSlot& Slot, const FMultimonitorMonitorInfo& MonitorInfo);
	void ApplyPostProcessToRenderTargetCaptures(UWorld* World, UTextureRenderTarget2D* RenderTarget, const TArray<FMultimonitorPostProcessEntry>& Materials, int32 MonitorIndex);
	void DestroyCaptureForMonitor(int32 MonitorIndex);
	void DestroyWindowForMonitor(int32 MonitorIndex);
	UWorld* GetPlayWorld() const;
};
