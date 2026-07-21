#include "MultimonitorRenderTargetWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UMultimonitorRenderTargetWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDisplayImage();
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
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::SetDisplayMaterial(UMaterialInterface* InMaterial, FName TextureParameterName)
{
	DisplayMaterial = InMaterial;
	TextureParamName = TextureParameterName;
	DisplayMID = nullptr;
	ApplyToImage();
}

void UMultimonitorRenderTargetWidget::ApplyToImage()
{
	EnsureDisplayImage();
	if (!DisplayImage)
	{
		return;
	}

	if (DisplayMaterial && RenderTarget)
	{
		if (!DisplayMID)
		{
			DisplayMID = UMaterialInstanceDynamic::Create(DisplayMaterial, this);
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
		Brush.ImageSize = FVector2D(RenderTarget->SizeX, RenderTarget->SizeY);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		DisplayImage->SetBrush(Brush);
	}
	else
	{
		DisplayImage->SetBrush(FSlateBrush());
	}
}
