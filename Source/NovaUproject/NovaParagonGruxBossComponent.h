#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NovaVoiceTypes.h"
#include "NovaParagonGruxBossComponent.generated.h"

class ACharacter;
class ANovaClickMovePlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FNovaGruxCounterSuccessDelegate,
	ENovaBossCounterType,
	CounterType,
	ENovaVoiceCommand,
	WeaponUsed);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNovaGroggyStateDelegate);

/**
 * Paragon Grux 에셋 보스 전용 상쇄 소스.
 * Grux 보스 BP에 이 컴포넌트를 붙이고, 패턴 텔레그래프 시 RequestCounterWindow()를 호출하세요.
 */
UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaParagonGruxBossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaParagonGruxBossComponent();

	static const FName ParagonGruxBossTag;

	UPROPERTY(BlueprintAssignable, Category = "Nova|Boss|Grux")
	FNovaGruxCounterSuccessDelegate OnGruxCounterSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Nova|Boss|Grux")
	FNovaGroggyStateDelegate OnGroggyStarted;

	UPROPERTY(BlueprintAssignable, Category = "Nova|Boss|Grux")
	FNovaGroggyStateDelegate OnGroggyEnded;

	/** 상쇄 창을 열 수 있는 최대 거리 (플레이어 ↔ Grux 보스) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux")
	float MaxCounterDistance = 4500.0f;

	/** BeginPlay에서 ParagonGrux 메시 사용 여부를 검사합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux")
	bool bValidateParagonGruxMesh = true;

	/** BP에서 OnGruxCounterSucceeded를 안 붙였을 때 C++ 기본 스턴 반응 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux")
	bool bAutoApplyDefaultCounterStagger = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux", meta = (ClampMin = "0.1"))
	float DefaultCounterStaggerSeconds = 2.0f;

	/** 패턴 상쇄 성공 시 그로기(행동 불능) 지속 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux|Groggy", meta = (ClampMin = "0.1"))
	float GroggyDurationSeconds = 4.0f;

	/** 그로기 중 피격 대미지 배율 (1 = 기본, 2 = 2배) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux|Groggy", meta = (ClampMin = "1.0"))
	float GroggyDamageMultiplier = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nova|Boss|Grux|Groggy")
	bool bIsGroggy = false;

	UFUNCTION(BlueprintCallable, Category = "Nova|Boss|Grux")
	bool RequestCounterWindow(ENovaBossCounterType CounterType);

	/** 상쇄 성공 후 그로기 진입: 이동·패턴 정지, 피격 추가 대미지 */
	UFUNCTION(BlueprintCallable, Category = "Nova|Boss|Grux|Groggy")
	void EnterGroggy(float DurationSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Nova|Boss|Grux|Groggy")
	void ExitGroggy();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux|Groggy")
	bool IsGroggy() const { return bIsGroggy; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux|Groggy")
	float GetGroggyDamageMultiplier() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux|Groggy")
	static bool IsActorGroggy(const AActor* Actor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux|Groggy")
	static float GetIncomingDamageMultiplierForActor(const AActor* Actor);

	/** 상쇄 성공 시 기본 반응: 몽타주 중단·이동 정지. OnGruxCounterSucceeded BP에서도 호출 가능 */
	UFUNCTION(BlueprintCallable, Category = "Nova|Boss|Grux")
	void ApplyCounterStaggerReaction(float StaggerSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux")
	static bool IsParagonGruxBossActor(const AActor* Actor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux")
	static bool CanActorServeAsCounterBoss(
		const AActor* BossActor,
		const ACharacter* PlayerCharacter,
		float MaxDistance,
		FString& OutRejectReason);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nova|Boss|Grux")
	static AActor* FindNearestParagonGruxBoss(const UWorld* World, const FVector& Origin, float MaxDistance);

	void NotifyGruxCounterSucceeded(ENovaBossCounterType CounterType, ENovaVoiceCommand WeaponUsed);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleDefaultCounterReaction(ENovaBossCounterType CounterType, ENovaVoiceCommand WeaponUsed);

	void RestoreMovementAfterStagger();
	void HandleGroggyTimerExpired();

	bool ValidateGruxAssetOnOwner(FString& OutRejectReason) const;
	ANovaClickMovePlayerController* ResolveNovaPlayerController() const;

	FTimerHandle StaggerTimerHandle;
	float SavedMaxWalkSpeed = 0.0f;
};
