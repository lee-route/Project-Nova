#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NovaVoiceTypes.h"
#include "NovaBossPatternComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UNovaFloatingHealthBarComponent;
class UNovaParagonGruxBossComponent;

UENUM(BlueprintType)
enum class ENovaBossPatternState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Chasing UMETA(DisplayName = "Chasing"),
	Telegraphing UMETA(DisplayName = "Telegraphing"),
	Executing UMETA(DisplayName = "Executing"),
	Recovering UMETA(DisplayName = "Recovering"),
	Staggered UMETA(DisplayName = "Staggered")
};

USTRUCT(BlueprintType)
struct FNovaBossAttackPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	FName PatternId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	ENovaBossCounterType CounterType = ENovaBossCounterType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	float TelegraphSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	float RecoverySeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	float Damage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	float AttackRadius = 350.0f;

	/** 360 = 전방향 범위, 45 = 전방 45도 부채꼴. 0이면 각도 제한 없음(거리만). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern", meta = (ClampMin = "0", ClampMax = "360"))
	float AttackArcDegrees = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	bool bOpensCounterWindow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNovaBossPatternTelegraphDelegate, ENovaBossCounterType, CounterType, int32, PatternIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNovaBossPatternExecuteDelegate, ENovaBossCounterType, CounterType, int32, PatternIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNovaBossPatternCounterDelegate, ENovaBossCounterType, CounterType);

UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaBossPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaBossPatternComponent();

	UPROPERTY(BlueprintAssignable, Category = "Boss|Pattern")
	FNovaBossPatternTelegraphDelegate OnPatternTelegraphStarted;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Pattern")
	FNovaBossPatternExecuteDelegate OnPatternExecuted;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Pattern")
	FNovaBossPatternCounterDelegate OnPatternCountered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	bool bAutoStartOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	bool bRotatePatternsInOrder = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	float AggroRadius = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	float ChaseRadius = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	float AttackRadius = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	float ChaseMoveSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|AI")
	float StaggerDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Pattern")
	TArray<FNovaBossAttackPattern> AttackPatterns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Visual")
	bool bShowPatternVisuals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Visual")
	float VisualGroundOffset = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Visual")
	float TelegraphPulseSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Visual")
	float ExecuteVisualSeconds = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Visual")
	float ChargeDashStrength = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Health")
	bool bAutoAddHealthBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Health", meta = (ClampMin = "1"))
	float BossMaxHealth = 1000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Boss|Pattern")
	ENovaBossPatternState CurrentState = ENovaBossPatternState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Boss|Pattern")
	int32 ActivePatternIndex = INDEX_NONE;

	UFUNCTION(BlueprintCallable, Category = "Boss|AI")
	void StartBossAI();

	UFUNCTION(BlueprintCallable, Category = "Boss|AI")
	void StopBossAI();

	/** Pattern_1~4 기본 패턴 채우기 (돌진/범위/투사체/360) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void InitializeDefaultGruxPatterns();

	/** C++ 패턴 드라이버: 텔레그래프 시 RequestCounterWindow 자동 호출 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void NotifyPatternTelegraph(ENovaBossCounterType CounterType);

	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void ForceNextPattern();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleGruxCounterSucceeded(ENovaBossCounterType CounterType, ENovaVoiceCommand Command);

	void BindGruxCounterDelegate();
	void UnbindGruxCounterDelegate();
	UNovaParagonGruxBossComponent* ResolveGruxBossComponent() const;
	bool EnsureGruxBossComponent();
	void EnsureBossHealthBar();

	ACharacter* GetOwnerCharacter() const;
	APawn* GetPlayerPawn() const;
	float GetDistanceToPlayer() const;
	void FacePlayer();

	void UpdateIdle();
	void UpdateChasing(float DeltaTime);
	void UpdateTelegraphing(float DeltaTime);
	void UpdateExecuting();
	void UpdateRecovering(float DeltaTime);
	void UpdateStaggered(float DeltaTime);

	void BeginTelegraph(int32 PatternIndex);
	void ExecuteActivePattern();
	void EnterStaggered();
	void AdvancePatternIndex();

	void EnsurePatternVisualMeshes();
	void HidePatternVisuals();
	void UpdateTelegraphVisual(float DeltaTime);
	void PlayExecuteVisual(const FNovaBossAttackPattern& Pattern);
	void UpdateExecuteVisual(float DeltaTime);
	FVector GetGroundLocationAt(const FVector& WorldLocation) const;

	bool IsFightActive() const { return bFightActive; }
	bool IsPlayerInAggroRange() const;
	bool IsPlayerInAttackRange() const;

	const FNovaBossAttackPattern* GetActivePattern() const;

	bool bFightActive = false;
	bool bCounteredThisPattern = false;
	float StateTimer = 0.0f;
	int32 NextPatternIndex = 0;

	bool bPatternVisualMeshesReady = false;
	bool bExecuteVisualActive = false;
	float ExecuteVisualTimer = 0.0f;
	float TelegraphVisualPulse = 0.0f;
	FVector ProjectileVisualStart = FVector::ZeroVector;
	FVector ProjectileVisualEnd = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> AreaVisualMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> AimVisualMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ImpactVisualMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TelegraphVisualMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExecuteVisualMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UNovaParagonGruxBossComponent> GruxBossComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNovaFloatingHealthBarComponent> BossHealthBarComponent;

	bool bGruxIntegrationReady = false;
};
