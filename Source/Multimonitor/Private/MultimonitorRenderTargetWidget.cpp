#include "MultimonitorRenderTargetWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MultimonitorAlphaDisplay.h"
#include "MultimonitorLog.h"

void UMultimonitorRenderTargetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDisplayImage();
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureDisplayImage();
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::RefreshFromSubsystem()
{
	if (!RenderTarget || !DisplayImage)
	{
		return;
	}

	if (bVisualizeAlpha)
	{
		UpdateAlphaVisualization();
	}
	ApplyToImage();
}

TSharedRef<SWidget> UMultimonitorRenderTargetWidget::RebuildWidget()
{
	EnsureDisplayImage();
	return Super::RebuildWidget();
}

void UMultimonitorRenderTargetWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::EnsureDisplayImage()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Root;
	}

	if (DisplayImage)
	{
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root)
	{
		return;
	}

	DisplayImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DisplayImage"));
	if (UCanvasPanelSlot* ImageSlot = Root->AddChildToCanvas(DisplayImage))
	{
		ImageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		ImageSlot->SetOffsets(FMargin(0.f));
	}
}

void UMultimonitorRenderTargetWidget::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	RenderTarget = InRenderTarget;
	if (bVisualizeAlpha)
	{
		UpdateAlphaVisualization();
	}
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::SetDisplayMaterial(UMaterialInterface* InMaterial, FName TextureParameterName)
{
	DisplayMaterial = InMaterial;
	TextureParamName = TextureParameterName;
	DisplayMID = nullptr;
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::SetVisualizeAlpha(bool bEnabled, bool bInvert)
{
	bVisualizeAlpha = bEnabled;
	bInvertAlpha = bInvert;
	DisplayMID = nullptr;
	if (!bVisualizeAlpha)
	{
		AlphaVizTexture = nullptr;
		AlphaPixelScratch.Reset();
	}
	else
	{
		UpdateAlphaVisualization();
	}
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::UpdateAlphaVisualization()
{
	if (!bVisualizeAlpha || !RenderTarget)
	{
		return;
	}

	UTexture2D* Texture = AlphaVizTexture.Get();
	UTexture2D* Result = MultimonitorAlphaDisplay::CopyAlphaToTexture(
		this,
		RenderTarget,
		Texture,
		AlphaPixelScratch,
		bInvertAlpha);
	AlphaVizTexture = Texture;

	if (!Result)
	{
		static bool bLogged = false;
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(LogMultimonitor, Warning, TEXT("Multimonitor: Failed to copy alpha into visualization texture."));
		}
	}
}

void UMultimonitorRenderTargetWidget::ApplyToImage()
{
	EnsureDisplayImage();
	if (!DisplayImage)
	{
		return;
	}

	if (bVisualizeAlpha)
	{
		DisplayMID = nullptr;
		UTexture2D* VizTex = AlphaVizTexture.Get();
		if (VizTex)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(VizTex);
			Brush.ImageSize = FVector2D(
				VizTex->GetSizeX() > 0 ? VizTex->GetSizeX() : 1920,
				VizTex->GetSizeY() > 0 ? VizTex->GetSizeY() : 1080);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.Tiling = ESlateBrushTileType::NoTile;
			Brush.Mirroring = ESlateBrushMirrorType::NoMirror;
			Brush.ImageType = ESlateBrushImageType::FullColor;
			DisplayImage->SetBrush(Brush);
			DisplayImage->SetBrushResourceObject(VizTex);
		}
		else
		{
			DisplayImage->SetBrush(FSlateBrush());
		}
		return;
	}

	UMaterialInterface* MaterialToUse = DisplayMaterial.Get();
	if (MaterialToUse && RenderTarget)
	{
		if (!DisplayMID || DisplayMID->Parent != MaterialToUse)
		{
			DisplayMID = UMaterialInstanceDynamic::Create(MaterialToUse, this);
		}

		if (DisplayMID)
		{
			DisplayMID->SetTextureParameterValue(TextureParamName, RenderTarget);
			DisplayImage->SetBrushFromMaterial(DisplayMID);
		}
		return;
	}

	DisplayMID = nullptr;

	if (RenderTarget)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(RenderTarget);
		Brush.ImageSize = FVector2D(
			RenderTarget->SizeX > 0 ? RenderTarget->SizeX : 1920,
			RenderTarget->SizeY > 0 ? RenderTarget->SizeY : 1080);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.Mirroring = ESlateBrushMirrorType::NoMirror;
		Brush.ImageType = ESlateBrushImageType::FullColor;
		DisplayImage->SetBrush(Brush);
		DisplayImage->SetBrushResourceObject(RenderTarget);
	}
	else
	{
		DisplayImage->SetBrush(FSlateBrush());
	}
}
