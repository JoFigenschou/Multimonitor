#include "MultimonitorWindow.h"

#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SNullWidget.h"

bool UMultimonitorWindow::Open(UWorld* World, int32 InMonitorIndex, const FVector2D& ScreenPosition, const FVector2D& ClientSize, bool bFullscreen, const FText& Title)
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	Close();

	MonitorIndex = InMonitorIndex;

	// Use borderless fixed windows positioned on each display.
	// EWindowMode::WindowedFullscreen is effectively single-display and breaks multi-output.
	SlateWindow = SNew(SWindow)
		.Title(Title)
		.Type(EWindowType::Normal)
		.ScreenPosition(ScreenPosition)
		.ClientSize(ClientSize)
		.AutoCenter(EAutoCenter::None)
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(!bFullscreen)
		.CreateTitleBar(!bFullscreen)
		.UseOSWindowBorder(!bFullscreen)
		.IsInitiallyMaximized(false)
		.FocusWhenFirstShown(false)
		.ActivationPolicy(EWindowActivationPolicy::Never)
		.IsTopmostWindow(false);

	if (!SlateWindow.IsValid())
	{
		return false;
	}

	FSlateApplication::Get().AddWindow(SlateWindow.ToSharedRef(), true);

	// Keep Windowed mode; size/position already match the target monitor.
	SlateWindow->SetWindowMode(EWindowMode::Windowed);
	SlateWindow->MoveWindowTo(ScreenPosition);
	SlateWindow->Resize(ClientSize);
	SlateWindow->ShowWindow();
	SlateWindow->BringToFront(false);
	return true;
}

void UMultimonitorWindow::SetContentWidget(UUserWidget* InWidget)
{
	ContentWidget = InWidget;

	if (!SlateWindow.IsValid())
	{
		return;
	}

	if (ContentWidget)
	{
		SlateWindow->SetContent(ContentWidget->TakeWidget());
	}
	else
	{
		SlateWindow->SetContent(SNullWidget::NullWidget);
	}
}

void UMultimonitorWindow::Close()
{
	if (SlateWindow.IsValid())
	{
		SlateWindow->RequestDestroyWindow();
		SlateWindow.Reset();
	}

	ContentWidget = nullptr;
	MonitorIndex = INDEX_NONE;
}
