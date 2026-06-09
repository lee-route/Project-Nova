#include "NovaMonsterHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"

void UNovaMonsterHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!HealthBar && WidgetTree)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
		Root->AddChild(HealthBar);

		if (UCanvasPanelSlot* HealthBarSlot = Cast<UCanvasPanelSlot>(HealthBar->Slot))
		{
			HealthBarSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			HealthBarSlot->SetOffsets(FMargin(0.0f));
		}

		HealthBar->SetFillColorAndOpacity(FLinearColor(0.85f, 0.1f, 0.1f, 1.0f));
		HealthBar->SetPercent(1.0f);
	}
}

void UNovaMonsterHealthBarWidget::UpdateHealth(const float Percent)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}
