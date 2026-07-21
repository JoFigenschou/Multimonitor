#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Widgets/SWindow.h"
#include "MultimonitorWindow.generated.h"

class UUserWidget;
class UWorld;

/**
 * Owns a secondary Slate window and its UMG content for one monitor slot.
 */
UCLASS(BlueprintType)
class MULTIMONITOR_API UMultimonitorWindow : public UObject
{
	GENERATED_BODY()

public:
	bool Open(UWorld* World, int32 InMonitorIndex, const FVector2D& ScreenPosition, const FVector2D& ClientSize, bool bFullscreen, const FText& Title);
	void SetContentWidget(UUserWidget* InWidget);
	void Close();

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	bool IsOpen() const { return SlateWindow.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	int32 GetMonitorIndex() const { return MonitorIndex; }

	UFUNCTION(BlueprintPure, Category = "Multimonitor")
	UUserWidget* GetContentWidget() const { return ContentWidget; }

	TSharedPtr<SWindow> GetSlateWindow() const { return SlateWindow; }

protected:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ContentWidget;

	TSharedPtr<SWindow> SlateWindow;
	int32 MonitorIndex = INDEX_NONE;
};
