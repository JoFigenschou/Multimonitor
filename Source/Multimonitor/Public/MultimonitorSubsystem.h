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

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	UMultimonitorWindow* GetWindowForMonitor(int32 MonitorIndex) const;

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	int32 GetPrimaryMonitorIndex() const;

protected:
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMultimonitorWindow>> Windows;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<AMultimonitorCaptureActor>> CaptureActors;

	UPROPERTY(Transient)
	TObjectPtr<UMultimonitorLayout> ActiveLayout;

	bool TryGetMonitorMetrics(int32 MonitorIndex, FMultimonitorMonitorInfo& OutInfo) const;
	bool IsPrimaryGameMonitor(int32 MonitorIndex) const;
	bool CreateOrUpdateSlot(const FMultimonitorSlot& Slot);
	UUserWidget* BuildContentWidget(UWorld* World, const FMultimonitorSlot& Slot, const FMultimonitorMonitorInfo& MonitorInfo);
	UTextureRenderTarget2D* EnsureCameraCapture(UWorld* World, const FMultimonitorSlot& Slot, const FMultimonitorMonitorInfo& MonitorInfo);
	void DestroyCaptureForMonitor(int32 MonitorIndex);
	void DestroyWindowForMonitor(int32 MonitorIndex);
	UWorld* GetPlayWorld() const;
};
