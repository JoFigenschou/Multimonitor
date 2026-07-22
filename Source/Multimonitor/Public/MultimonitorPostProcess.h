#pragma once

#include "CoreMinimal.h"
#include "MultimonitorTypes.h"

class USceneCaptureComponent2D;
class UCameraComponent;
class UMaterialInterface;
struct FPostProcessSettings;

namespace MultimonitorPostProcess
{
	/** FinalToneCurveHDR + post-processing + eye adaptation show flags. */
	void ConfigureForViewportMatch(USceneCaptureComponent2D* Capture);

	/** Enable post-processing without changing CaptureSource. */
	void EnsurePostProcessEnabled(USceneCaptureComponent2D* Capture);

	/**
	 * Copy camera look into Settings and force exposure overrides so SceneCapture
	 * actually applies them (volumes alone often miss captures).
	 */
	void CopyCameraLookToSettings(const UCameraComponent* Camera, FPostProcessSettings& InOutSettings);

	/** Refresh exposure fields from the camera onto an existing capture (cheap per-tick sync). */
	void SyncExposureFromCamera(USceneCaptureComponent2D* Capture, const UCameraComponent* Camera);

	/**
	 * Applies slot post-process materials as SceneCapture blendables.
	 * Materials must be Post Process domain. Does not clear blendables already on the capture.
	 */
	int32 ApplyMaterials(
		USceneCaptureComponent2D* Capture,
		const TArray<FMultimonitorPostProcessEntry>& Materials);

	UMaterialInterface* ResolveMaterial(const TSoftObjectPtr<UMaterialInterface>& SoftMaterial);
}
