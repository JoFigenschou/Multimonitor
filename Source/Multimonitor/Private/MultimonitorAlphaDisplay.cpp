#include "MultimonitorAlphaDisplay.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "MultimonitorLog.h"
#include "Rendering/Texture2DResource.h"

namespace MultimonitorAlphaDisplay
{
	static UTexture2D* EnsureTexture(UObject* Outer, UTexture2D*& InOutTexture, int32 SizeX, int32 SizeY)
	{
		if (InOutTexture
			&& InOutTexture->GetSizeX() == SizeX
			&& InOutTexture->GetSizeY() == SizeY)
		{
			return InOutTexture;
		}

		UTexture2D* NewTex = UTexture2D::CreateTransient(SizeX, SizeY, PF_B8G8R8A8);
		if (!NewTex)
		{
			return nullptr;
		}

		if (Outer)
		{
			NewTex->Rename(nullptr, Outer, REN_DoNotDirty | REN_NonTransactional);
		}

		NewTex->CompressionSettings = TC_VectorDisplacementmap;
		NewTex->SRGB = false;
		NewTex->Filter = TF_Bilinear;
		NewTex->AddressX = TA_Clamp;
		NewTex->AddressY = TA_Clamp;
		NewTex->NeverStream = true;
		NewTex->UpdateResource();
		InOutTexture = NewTex;
		return InOutTexture;
	}

	UTexture2D* CopyAlphaToTexture(
		UObject* WorldContextObject,
		UTextureRenderTarget2D* Source,
		UTexture2D*& InOutTexture,
		TArray<FColor>& InOutPixelScratch,
		bool bInvertAlpha)
	{
		if (!WorldContextObject || !Source)
		{
			return nullptr;
		}

		const int32 SizeX = Source->SizeX;
		const int32 SizeY = Source->SizeY;
		if (SizeX <= 0 || SizeY <= 0)
		{
			return nullptr;
		}

		TArray<FLinearColor> Samples;
		if (!UKismetRenderingLibrary::ReadRenderTargetRaw(WorldContextObject, Source, Samples, /*bNormalize=*/false))
		{
			UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: ReadRenderTargetRaw failed for '%s'."), *Source->GetName());
			return nullptr;
		}

		if (Samples.Num() != SizeX * SizeY)
		{
			UE_LOG(LogMultimonitor, Warning,
				TEXT("Multimonitor: Alpha read size mismatch for '%s' (%d samples, expected %dx%d)."),
				*Source->GetName(), Samples.Num(), SizeX, SizeY);
			return nullptr;
		}

		UTexture2D* Texture = EnsureTexture(WorldContextObject, InOutTexture, SizeX, SizeY);
		if (!Texture)
		{
			return nullptr;
		}

		InOutPixelScratch.SetNumUninitialized(Samples.Num());
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			float A = Samples[Index].A;
			if (bInvertAlpha)
			{
				A = 1.0f - A;
			}
			A = FMath::Clamp(A, 0.0f, 1.0f);
			const uint8 V = static_cast<uint8>(FMath::RoundToInt(A * 255.0f));
			InOutPixelScratch[Index] = FColor(V, V, V, 255);
		}

		// Heap copy so render-thread upload stays valid after this returns.
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, SizeX, SizeY);
		TArray<FColor>* UploadPixels = new TArray<FColor>(MoveTemp(InOutPixelScratch));
		InOutPixelScratch.Reset();

		Texture->UpdateTextureRegions(
			0,
			1,
			Region,
			static_cast<uint32>(SizeX * sizeof(FColor)),
			sizeof(FColor),
			reinterpret_cast<uint8*>(UploadPixels->GetData()),
			[UploadPixels, Region](uint8* /*SrcData*/, const FUpdateTextureRegion2D* /*Regions*/)
			{
				delete UploadPixels;
				delete Region;
			});

		return Texture;
	}
}
