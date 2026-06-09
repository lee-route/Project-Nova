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

	/** 상쇄 창을 열 수 있는 최대 거리 (플레이어 ↔ Grux 보스) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux")
	float MaxCounterDistance = 4500.0f;

	/** BeginPlay에서 ParagonGrux 메시 사용 여부를 검사합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nova|Boss|Grux")
	bool bValidateParagonGruxMesh = true;

	UFUNCTION(BlueprintCallable, Category = "Nova|Boss|Grux")
	bool RequestCounterWindow(ENovaBossCounterType CounterType);

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
	bool ValidateGruxAssetOnOwner(FString& OutRejectReason) const;
	ANovaClickMovePlayerController* ResolveNovaPlayerController() const;
};
