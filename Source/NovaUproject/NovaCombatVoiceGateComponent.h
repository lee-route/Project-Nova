#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NovaVoiceTypes.h"
#include "NovaCombatVoiceGateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNovaCounterSuccessDelegate, ENovaBossCounterType, CounterType, ENovaVoiceCommand, Command);

UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaCombatVoiceGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaCombatVoiceGateComponent();
	UPROPERTY(BlueprintAssignable, Category = "Voice|Combat")
	FNovaCounterSuccessDelegate OnCounterSucceeded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Combat")
	float CounterWindowSeconds = 1.2f;

	UPROPERTY(BlueprintReadOnly, Category = "Voice|Combat")
	bool bCounterWindowOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Voice|Combat")
	ENovaBossCounterType ActiveCounterType = ENovaBossCounterType::None;

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	void OpenCounterWindow(ENovaBossCounterType CounterType, float OverrideWindowSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	void CloseCounterWindow();

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	bool TryAcceptVoiceCommand(const FNovaVoiceCommandResult& CommandResult, FString& OutRejectReason);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float RemainingWindowSeconds = 0.0f;

	static ENovaVoiceCommand GetRequiredCommandForCounter(ENovaBossCounterType CounterType);
	static bool IsWeaponSwitchCommand(ENovaVoiceCommand Command);
};
