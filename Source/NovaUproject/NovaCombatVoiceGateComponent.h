#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NovaVoiceTypes.h"
#include "NovaCombatVoiceGateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNovaCounterSuccessDelegate, ENovaBossCounterType, CounterType, ENovaVoiceCommand, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNovaCounterWindowOpenedDelegate, ENovaBossCounterType, CounterType);

UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaCombatVoiceGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaCombatVoiceGateComponent();
	UPROPERTY(BlueprintAssignable, Category = "Voice|Combat")
	FNovaCounterSuccessDelegate OnCounterSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Voice|Combat")
	FNovaCounterWindowOpenedDelegate OnCounterWindowOpened;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Combat")
	float CounterWindowSeconds = 1.2f;

	/** Grux 보스와 이 거리 안에서만 상쇄 창 유지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Combat")
	float MaxCounterBossDistance = 4500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Voice|Combat")
	bool bCounterWindowOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Voice|Combat")
	ENovaBossCounterType ActiveCounterType = ENovaBossCounterType::None;

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	bool OpenCounterWindow(
		ENovaBossCounterType CounterType,
		AActor* BossSource,
		float OverrideWindowSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	AActor* GetActiveCounterBossSource() const { return ActiveCounterBossSource.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	void CloseCounterWindow();

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	bool TryAcceptVoiceCommand(const FNovaVoiceCommandResult& CommandResult, FString& OutRejectReason);

	/** Call after weapon switch. Returns false when counter window is open but wrong weapon was spoken. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	bool TryResolveCounterWindow(const FNovaVoiceCommandResult& CommandResult, FString& OutRejectReason);

	/** 키보드 무기 전환(1~4) 후 상쇄 판정. 창이 열려 있지 않으면 true(무시). */
	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	bool TryCounterWithEquippedWeapon(ENovaVoiceCommand EquippedWeapon, FString& OutRejectReason);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	static ENovaVoiceCommand GetRequiredWeaponForPattern(ENovaBossCounterType CounterType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	static FString GetPatternCodeName(ENovaBossCounterType CounterType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	static FString GetPatternDisplayName(ENovaBossCounterType CounterType);

	/** 예: 돌진(Pattern_1) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	static FString GetPatternFullLabel(ENovaBossCounterType CounterType);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	static FString GetRequiredWeaponDisplayName(ENovaBossCounterType CounterType);

	/** 예: 방패로 상쇄 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice|Combat")
	static FString GetRequiredWeaponCounterLabel(ENovaBossCounterType CounterType);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float RemainingWindowSeconds = 0.0f;

	TWeakObjectPtr<AActor> ActiveCounterBossSource;

	bool ValidateCounterBossSource(AActor* BossSource, FString& OutRejectReason) const;
	bool IsActiveCounterBossStillValid(FString& OutRejectReason) const;
	void InvalidateCounterBossIfNeeded();

	static ENovaVoiceCommand GetRequiredCommandForCounter(ENovaBossCounterType CounterType);
	static bool IsWeaponSwitchCommand(ENovaVoiceCommand Command);
	static FString GetWeaponDisplayNameFromCommand(ENovaVoiceCommand Command);
	bool AcceptCounterSuccess(ENovaVoiceCommand Command);
};
