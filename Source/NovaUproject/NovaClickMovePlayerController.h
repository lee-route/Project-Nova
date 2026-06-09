#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NovaVoiceTypes.h"
#include "NovaClickMovePlayerController.generated.h"

class UNavigationSystemV1;
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

	/** Paragon Grux 보스만 CounterType 상쇄 창을 열 수 있습니다. */
	UFUNCTION(BlueprintCallable, Category = "Voice|Combat")
	bool OpenBossCounterWindow(ENovaBossCounterType CounterType, AActor* BossSource);

	UFUNCTION(BlueprintCallable, Category = "Voice")
	UNovaVoiceCaptureComponent* GetVoiceCaptureComponent() const { return VoiceCaptureComponent; }

	/** BP: 무기 메시·몽타주·이펙트 연출 (C++는 enum 상태만 갱신) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Visual")
	void OnSecondaryWeaponChanged(ENovaVoiceCommand NewWeapon);

	/** BP: 보스 상쇄 성공 연출 (C++는 판정만 처리) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Visual")
	void OnBossCounterVisualSuccess(ENovaBossCounterType CounterType, ENovaVoiceCommand WeaponUsed);

	/** BP: 보스 패턴 인식 후 상쇄 창 UI (연출·표시) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Combat")
	void OnBossCounterWindowOpened(ENovaBossCounterType CounterType, ENovaVoiceCommand RequiredWeapon);

	/** BP: "도와줘" 연출 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice|Visual")
	void OnCompanionHelpVisualRequested();

	/** bp_npc / BPI_Interactable — WBP_Dialogue (공유 프로젝트와 동일 API) */
	UFUNCTION(BlueprintCallable, Category = "Nova|Dialogue")
	void StartDialogueMode(AActor* NpcActor = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Nova|Dialogue")
	void EndDialogueMode();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
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

protected:
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

private:
	void OnRightClickPressed();
	void OnRightClickReleased();
	void UpdateDestinationUnderCursor(bool bPrintDebug);
	bool GetWalkableHitUnderCursor(FHitResult& OutHit) const;
	bool ProjectCursorToWalkablePoint(FVector& InOutWorldPoint, FHitResult& OutHit) const;
	bool FindWalkableHitOnCameraRay(const FVector& PlanePoint, FHitResult& OutHit) const;
	bool SnapDestinationToNavMesh(FVector& InOutLocation) const;
	void RefreshNavPath();
	bool ShouldUseNavMeshPath(const APawn* InPawn, const UNavigationSystemV1* NavSys) const;
	FVector GetActiveMoveTarget() const;
	static bool IsStairSurfaceHit(const FHitResult& Hit);
	static bool IsWalkableSurfaceHit(const FHitResult& Hit, float MinNormalZ);
	void ApplyFixedOrbitCamera();
	void ApplyGameplayInputMode();
	void ApplyMovementRotationSettings(APawn* InPawn);
	void ConsumeLookAxis(float Value);
	void TickDialogueInput();
	bool IsDialogueActive() const;
	void TryStartNpcDialogue();

	void OnDashPressed();
	void OnJumpPressed();

	void OnSkillQ();
	void OnSkillW();
	void OnSkillE();
	void OnSkillR();

	void SpawnClickMoveIndicator(const FVector& WorldLocation);

	bool bHasDestination = false;

	FVector Destination = FVector::ZeroVector;

	bool bIsHoldingMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float AcceptanceRadius = 50.0f;

	/** 바닥 판정: Impact Normal Z >= 이 값 (0.55 ≈ 57° 이하 기울기) */
	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float WalkableMinNormalZ = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float ClickMoveTraceDistance = 20000.0f;

	/** 커서 XY → 위에서 아래로 바닥 탐색 (건축물 관통 방지) */
	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float ClickMoveVerticalTraceUp = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float ClickMoveVerticalTraceDown = 3000.0f;

	/** 커서→평면 투영점과 XY가 이 거리 이내인 바닥만 인정 (아치 너머 바닥 오선택 방지) */
	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float ClickMoveMaxXYSlop = 280.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float NavProjectionExtent = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float NavProjectionVerticalExtent = 2000.0f;

	/** 계단·경사면 오르기 (캐릭터 이동) */
	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float MaxStepHeight = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float MaxWalkableFloorAngle = 52.0f;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	bool bUseNavMeshPathfinding = true;

	TArray<FVector> NavPathPoints;
	int32 NavPathIndex = 0;

	bool bDestinationRequiresDirectMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float FixedOrbitArmLength = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float FixedOrbitPitch = -45.0f;

	/** 메시 기본 방향 보정 (3시→12시: -90) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CharacterFacingYawOffset = -90.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Knight")
	bool bUseMedievalKnightVisual = true;

	UPROPERTY(EditDefaultsOnly, Category = "Character|Knight")
	FVector KnightMeshRelativeLocation = FVector(0.f, 0.f, -88.f);

	UPROPERTY(EditDefaultsOnly, Category = "Character|Knight")
	FRotator KnightMeshRelativeRotation = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Nova|Dialogue")
	float NpcInteractRadius = 350.0f;

	// If true, show cursor while RMB is held for click-move.
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
	void OnCounterWindowOpened(ENovaBossCounterType CounterType);
	void NotifyCounterAfterWeaponSwitch(ENovaVoiceCommand WeaponCommand);

	void ApplyWeaponVisualToPawn(ENovaVoiceCommand WeaponCommand);
	void ApplyMedievalKnightVisual(APawn* InPawn);

	void TurnCamera(float AxisValue);
	void ApplyCameraYaw(float DeltaTime);
	bool IsFirstPersonDungeonPawn() const;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraTurnSpeed = 90.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Camera")
	float CameraYawDegrees = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Nova|Dialogue")
	TObjectPtr<UUserWidget> ActiveDialogueWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Nova|Dialogue")
	TSubclassOf<UUserWidget> DialogueWidgetClass;

	UPROPERTY(VisibleInstanceOnly, Category = "Nova|Dialogue")
	TObjectPtr<AActor> ActiveDialogueNpc;

	bool bPrevGKeyDown = false;

	bool bCameraYawLeft = false;
	bool bCameraYawRight = false;

	void OnWeaponKey1();
	void OnWeaponKey2();
	void OnWeaponKey3();
	void OnWeaponKey4();
	void OnDebugCounterKeyF5();
	void OnDebugCounterKeyF6();
	void OnDebugCounterKeyF7();
	void OnDebugCounterKeyF8();
	bool TryOpenDebugCounterWindow(ENovaBossCounterType CounterType);
};
