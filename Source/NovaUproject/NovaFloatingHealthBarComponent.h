#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NovaFloatingHealthBarComponent.generated.h"

class UUserWidget;
class UWidgetComponent;

UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaFloatingHealthBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaFloatingHealthBarComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|UI")
	bool bAutoCreateWidgetIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|UI")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|UI")
	FName WidgetComponentName = TEXT("NovaMonsterHealthBar");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|UI")
	FVector BarRelativeLocation = FVector(0.0f, 0.0f, 120.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|UI")
	FVector2D BarDrawSize = FVector2D(120.0f, 12.0f);

	/** 보스용 큰 체력바 프리셋 적용 */
	UFUNCTION(BlueprintCallable, Category = "Nova|Health")
	void ApplyBossHealthBarPreset(float InMaxHealth = 1000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Health")
	bool bDestroyOwnerOnDeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Health")
	float DeathDestroyDelay = 2.0f;

	UFUNCTION(BlueprintCallable, Category = "Nova|Health")
	void InitializeHealth();

	/** BeginPlay 이후 런타임에 붙일 때 위젯·데미지 바인딩 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Nova|Health")
	void ActivateHealthBar();

	UFUNCTION(BlueprintCallable, Category = "Nova|Health")
	void RefreshHealthBar();

	UFUNCTION(BlueprintPure, Category = "Nova|Health")
	float GetHealthPercent() const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleAnyDamage(
		AActor* DamagedActor,
		float Damage,
		const class UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser);

	UWidgetComponent* ResolveWidgetComponent();
	void EnsureWidgetComponent();
	void ApplyHealthBarPercent(float Percent);
	void HandleDeath();

	UPROPERTY()
	TObjectPtr<UWidgetComponent> CachedWidgetComponent;

	bool bCreatedWidgetComponent = false;
};
