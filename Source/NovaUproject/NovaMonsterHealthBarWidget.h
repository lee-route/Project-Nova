#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NovaMonsterHealthBarWidget.generated.h"

class UProgressBar;

UCLASS(Blueprintable, BlueprintType)
class NOVAUPROJECT_API UNovaMonsterHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Nova|Health")
	void UpdateHealth(float Percent);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;
};
