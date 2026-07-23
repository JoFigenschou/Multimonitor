#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UTextureRenderTarget2D;

/** Copy RT.A into a transient Texture2D as RGB (R/G/B = A) for display. */
namespace MultimonitorAlphaDisplay
{
	/**
	 * Reads Source.A (optionally inverted) into InOutTexture as grayscale RGB.
	 * Creates/resizes InOutTexture as needed. Returns the texture, or nullptr on failure.
	 */
	UTexture2D* CopyAlphaToTexture(
		UObject* WorldContextObject,
		UTextureRenderTarget2D* Source,
		UTexture2D*& InOutTexture,
		TArray<FColor>& InOutPixelScratch,
		bool bInvertAlpha);
}
