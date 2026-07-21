#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MultimonitorTypes.h"
#include "MultimonitorLayout.generated.h"

/**
 * Data asset describing which content appears on which monitors.
 * Apply via UMultimonitorSubsystem::ApplyLayout.
 */
UCLASS(BlueprintType)
class MULTIMONITOR_API UMultimonitorLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Multimonitor")
	TArray<FMultimonitorSlot> Slots;
};
