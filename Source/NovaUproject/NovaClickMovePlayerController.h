#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NovaVoiceTypes.h"
#include "NovaClickMovePlayerController.generated.h"

class UFXSystemAsset;
class UNovaCombatVoiceGateComponent;
class UNovaVoiceCaptureComponent;
class UUserWidget;

struct FNovaVoiceCommandResult;

UENUM(BlueprintType)
enum class ENovaControlMode : uint8
{
	ClickMove UMETA(DisplayName = "Click Move"),
	WASD UMETA(DisplayName = "WASD")
};

UCLASS()
class NOVAUPROJECT_API ANovaClickMovePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANovaClickMovePlayerController();

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
	ENovaVoiceCommand GetEquippedSecondaryWeapon() const { return EquippedSecondaryWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
	bool SwitchSecondaryWeapon(ENovaVoiceCommand WeaponCommand);

	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	void OpenBossCounterWindow(ENovaBossCounterType CounterType);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	UNovaVoiceCaptureComponent* GetVoiceCaptureComponent() const { return VoiceCaptureComponent; }

	/** BP: 무기 메시·몽타주·이펙트 연출 (C++는 enum 상태만 갱신) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Visual")
	void OnSecondaryWeaponChanged(ENovaVoiceCommand NewWeapon);

	/** BP: 보스 상쇄 성공 연출 (C++는 판정만 처리) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Visual")
	void OnBossCounterVisualSuccess(ENovaBossCounterType CounterType, ENovaVoiceCommand WeaponUsed);

	/** BP: "도와줘" 연출 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Visual")
	void OnCompanionHelpVisualRequested();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void SetControlMode(ENovaControlMode NewMode);

	// Optional VFX spawned when setting a click-move destination.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ClickMove|VFX", meta = (AllowPrivateAccess = "true"))
	UFXSystemAsset* ClickMoveIndicatorFx = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ClickMove|VFX", meta = (AllowPrivateAccess = "true"))
	float ClickMoveIndicatorScale = 1.0f;
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill")
	void BP_UseSkillQ(ENovaVoiceCommand CurrentWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = "Skill")
	void BP_UseSkillW(ENovaVoiceCommand CurrentWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = "Skill")
	void BP_UseSkillE(ENovaVoiceCommand CurrentWeapon);

	UFUNCTION(BlueprintImplementableEvent, Category = "Skill")
	void BP_UseSkillR(ENovaVoiceCommand CurrentWeapon);

private:
	void OnLeftClickPressed();
	void OnLeftClickReleased();
	void UpdateDestinationUnderCursor(bool bPrintDebug);

	void OnVPressed();
	void OnDashPressed();

	void OnSkillQ();
	void OnSkillW();
	void OnSkillE();
	void OnSkillR();

	void MoveForward(float Value);
	void MoveRight(float Value);
	void ApplyTopDownCamera();
	void SpawnClickMoveIndicator(const FVector& WorldLocation);

	bool bHasDestination = false;

	FVector Destination = FVector::ZeroVector;

	bool bIsHoldingMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float AcceptanceRadius = 50.0f;

	// If true, show cursor only while LMB is held.
	UPROPERTY(EditDefaultsOnly, Category = "Cursor")
	bool bShowCursorWhileHoldingMove = true;

	// Optional: replace OS cursor with a UMG cursor widget (e.g. arrow image).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Cursor", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> CursorWidgetClass;

	UPROPERTY(VisibleInstanceOnly, Category = "Control")
	ENovaControlMode ControlMode = ENovaControlMode::ClickMove;

	UPROPERTY(VisibleInstanceOnly, Category = "Camera")
	bool bIsTopDownCamera = true;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashStrength = 1600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashUpwardStrength = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNovaVoiceCaptureComponent> VoiceCaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNovaCombatVoiceGateComponent> CombatVoiceGateComponent;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Weapon")
	ENovaVoiceCommand EquippedSecondaryWeapon = ENovaVoiceCommand::Shield;

	UFUNCTION()
	void OnVoiceCommandRecognized(const FNovaVoiceCommandResult& CommandResult);
	void HandleWeaponSwitchInput(ENovaVoiceCommand WeaponCommand);
	void RequestCompanionHelp();

	UFUNCTION()
	void OnCounterSucceeded(ENovaBossCounterType CounterType, ENovaVoiceCommand Command);

	void ApplyWeaponVisualToPawn(ENovaVoiceCommand WeaponCommand);

	void OnWeaponKey1();
	void OnWeaponKey2();
	void OnWeaponKey3();
	void OnWeaponKey4();
	void OnDebugCounterKeyF5();
	void OnDebugCounterKeyF6();
	void OnDebugCounterKeyF7();
	void OnDebugCounterKeyF8();
};
