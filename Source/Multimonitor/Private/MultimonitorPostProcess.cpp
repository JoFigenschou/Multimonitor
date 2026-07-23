#include "MultimonitorPostProcess.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Scene.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MultimonitorLog.h"

namespace MultimonitorPostProcess
{
	UMaterialInterface* ResolveMaterial(const TSoftObjectPtr<UMaterialInterface>& SoftMaterial)
	{
		if (SoftMaterial.IsNull())
		{
			return nullptr;
		}

		UMaterialInterface* Material = SoftMaterial.Get();
		if (!Material)
		{
			Material = SoftMaterial.LoadSynchronous();
		}
		return Material;
	}

	void EnsurePostProcessEnabled(USceneCaptureComponent2D* Capture)
	{
		if (!Capture)
		{
			return;
		}

		Capture->ShowFlags.SetPostProcessing(true);
		Capture->ShowFlags.SetEyeAdaptation(true);
		Capture->ShowFlags.SetToneCurve(true);
		if (Capture->PostProcessBlendWeight <= KINDA_SMALL_NUMBER)
		{
			Capture->PostProcessBlendWeight = 1.0f;
		}
	}

	void ConfigureForViewportMatch(USceneCaptureComponent2D* Capture)
	{
		if (!Capture)
		{
			return;
		}

		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
		Capture->bAlwaysPersistRenderingState = true;
		EnsurePostProcessEnabled(Capture);
	}

	void ConfigureForAlphaCapture(USceneCaptureComponent2D* Capture)
	{
		if (!Capture)
		{
			return;
		}

		static IConsoleVariable* PropagateAlphaCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
		if (PropagateAlphaCVar && PropagateAlphaCVar->GetInt() == 0)
		{
			UE_LOG(LogMultimonitor, Warning,
				TEXT("Multimonitor: Visualize Alpha needs r.PostProcessing.PropagateAlpha=1 (Project Settings → Rendering → Enable alpha channel support in post processing)."));
		}

		// Writes Inv Opacity into A. Display Invert Alpha converts that to opacity grayscale.
		Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->CompositeMode = SCCM_Overwrite;
		Capture->ShowFlags.SetPostProcessing(false);
		Capture->ShowFlags.SetTranslucency(true);
		Capture->ShowFlags.SetParticles(true);
		Capture->PostProcessBlendWeight = 0.0f;
		Capture->PostProcessSettings = FPostProcessSettings();

		if (UTextureRenderTarget2D* RT = Capture->TextureTarget)
		{
			// Inv Opacity clear: fully transparent = A 1.
			RT->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		}

		UE_LOG(LogMultimonitor, Log,
			TEXT("Multimonitor: Alpha capture on '%s' (SceneColorHDR Inv Opacity in A). Opaque UE5 meshes may need Holdout to punch A."),
			*Capture->GetName());
	}

	static void ForceExposureOverrides(FPostProcessSettings& Settings)
	{
		// SceneCapture ignores many volume-only defaults unless overrides are set locally.
		Settings.bOverride_AutoExposureMethod = true;
		Settings.bOverride_AutoExposureBias = true;
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.bOverride_AutoExposureSpeedUp = true;
		Settings.bOverride_AutoExposureSpeedDown = true;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;

		// Converge quickly so secondary captures track the camera look.
		Settings.AutoExposureSpeedUp = FMath::Max(Settings.AutoExposureSpeedUp, 20.0f);
		Settings.AutoExposureSpeedDown = FMath::Max(Settings.AutoExposureSpeedDown, 20.0f);
	}

	void CopyCameraLookToSettings(const UCameraComponent* Camera, FPostProcessSettings& InOutSettings)
	{
		if (!Camera)
		{
			return;
		}

		InOutSettings = Camera->PostProcessSettings;
		ForceExposureOverrides(InOutSettings);
	}

	void SyncExposureFromCamera(USceneCaptureComponent2D* Capture, const UCameraComponent* Camera)
	{
		if (!Capture || !Camera)
		{
			return;
		}

		FPostProcessSettings& Dst = Capture->PostProcessSettings;
		const FPostProcessSettings& Src = Camera->PostProcessSettings;

		Dst.bOverride_AutoExposureMethod = true;
		Dst.AutoExposureMethod = Src.bOverride_AutoExposureMethod ? Src.AutoExposureMethod : Dst.AutoExposureMethod;

		Dst.bOverride_AutoExposureBias = true;
		Dst.AutoExposureBias = Src.AutoExposureBias;

		Dst.bOverride_AutoExposureMinBrightness = true;
		Dst.AutoExposureMinBrightness = Src.bOverride_AutoExposureMinBrightness ? Src.AutoExposureMinBrightness : Dst.AutoExposureMinBrightness;

		Dst.bOverride_AutoExposureMaxBrightness = true;
		Dst.AutoExposureMaxBrightness = Src.bOverride_AutoExposureMaxBrightness ? Src.AutoExposureMaxBrightness : Dst.AutoExposureMaxBrightness;

		Dst.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Dst.AutoExposureApplyPhysicalCameraExposure = Src.AutoExposureApplyPhysicalCameraExposure;

		Dst.bOverride_AutoExposureSpeedUp = true;
		Dst.bOverride_AutoExposureSpeedDown = true;
		Dst.AutoExposureSpeedUp = FMath::Max(Src.AutoExposureSpeedUp, 20.0f);
		Dst.AutoExposureSpeedDown = FMath::Max(Src.AutoExposureSpeedDown, 20.0f);

		Capture->PostProcessBlendWeight = FMath::Clamp(Camera->PostProcessBlendWeight, 0.0f, 1.0f);
		if (Capture->PostProcessBlendWeight <= KINDA_SMALL_NUMBER)
		{
			Capture->PostProcessBlendWeight = 1.0f;
		}
	}

	int32 ApplyMaterials(
		USceneCaptureComponent2D* Capture,
		const TArray<FMultimonitorPostProcessEntry>& Materials)
	{
		if (!Capture || Materials.Num() == 0)
		{
			return 0;
		}

		ConfigureForViewportMatch(Capture);

		int32 Applied = 0;
		for (const FMultimonitorPostProcessEntry& Entry : Materials)
		{
			UMaterialInterface* Material = ResolveMaterial(Entry.Material);
			if (!Material)
			{
				if (!Entry.Material.IsNull())
				{
					UE_LOG(LogMultimonitor, Warning,
						TEXT("Multimonitor: Failed to load post-process material '%s'."),
						*Entry.Material.ToString());
				}
				continue;
			}

			if (const UMaterial* Base = Material->GetMaterial())
			{
				if (Base->MaterialDomain != MD_PostProcess)
				{
					UE_LOG(LogMultimonitor, Warning,
						TEXT("Multimonitor: '%s' is not Post Process domain (domain=%d). SceneCapture needs Post Process materials."),
						*Material->GetName(),
						static_cast<int32>(Base->MaterialDomain));
					continue;
				}
			}

			Capture->AddOrUpdateBlendable(Material, Entry.Weight);
			++Applied;
			UE_LOG(LogMultimonitor, Log,
				TEXT("Multimonitor: Applied PP material '%s' (weight=%.2f) to SceneCapture '%s'."),
				*Material->GetName(),
				Entry.Weight,
				*Capture->GetName());
		}

		return Applied;
	}
}
