#include "NovaParagonGruxBossComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "NovaClickMovePlayerController.h"

const FName UNovaParagonGruxBossComponent::ParagonGruxBossTag(TEXT("ParagonGruxBoss"));

UNovaParagonGruxBossComponent::UNovaParagonGruxBossComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNovaParagonGruxBossComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Owner->Tags.AddUnique(ParagonGruxBossTag);
	}

	if (bValidateParagonGruxMesh)
	{
		FString RejectReason;
		if (!ValidateGruxAssetOnOwner(RejectReason))
		{
			UE_LOG(LogTemp, Warning, TEXT("NovaParagonGruxBossComponent: %s"), *RejectReason);
		}
	}
}

bool UNovaParagonGruxBossComponent::ValidateGruxAssetOnOwner(FString& OutRejectReason) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutRejectReason = TEXT("Owner가 없습니다");
		return false;
	}

	const USkeletalMeshComponent* MeshComp = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		OutRejectReason = TEXT("Paragon Grux 보스에 스켈레탈 메시가 없습니다");
		return false;
	}

	const FString MeshPath = MeshComp->GetSkeletalMeshAsset()->GetPathName();
	if (!MeshPath.Contains(TEXT("ParagonGrux"), ESearchCase::IgnoreCase))
	{
		OutRejectReason = FString::Printf(
			TEXT("ParagonGrux 메시가 아닙니다: %s"),
			*MeshPath);
		return false;
	}

	return true;
}

bool UNovaParagonGruxBossComponent::IsParagonGruxBossActor(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	if (Actor->ActorHasTag(ParagonGruxBossTag))
	{
		return true;
	}

	return Actor->FindComponentByClass<UNovaParagonGruxBossComponent>() != nullptr;
}

bool UNovaParagonGruxBossComponent::CanActorServeAsCounterBoss(
	const AActor* BossActor,
	const ACharacter* PlayerCharacter,
	const float MaxDistance,
	FString& OutRejectReason)
{
	if (!IsParagonGruxBossActor(BossActor))
	{
		OutRejectReason = TEXT("상쇄는 Paragon Grux 보스에게만 사용할 수 있습니다");
		return false;
	}

	if (!BossActor || BossActor->IsPendingKillPending())
	{
		OutRejectReason = TEXT("상쇄 대상 Grux 보스가 유효하지 않습니다");
		return false;
	}

	if (!PlayerCharacter)
	{
		OutRejectReason = TEXT("플레이어 캐릭터를 찾을 수 없습니다");
		return false;
	}

	const float DistanceSq = FVector::DistSquared(BossActor->GetActorLocation(), PlayerCharacter->GetActorLocation());
	if (DistanceSq > FMath::Square(MaxDistance))
	{
		OutRejectReason = TEXT("Grux 보스와 거리가 너무 멉니다");
		return false;
	}

	return true;
}

AActor* UNovaParagonGruxBossComponent::FindNearestParagonGruxBoss(
	const UWorld* World,
	const FVector& Origin,
	const float MaxDistance)
{
	if (!World)
	{
		return nullptr;
	}

	AActor* NearestBoss = nullptr;
	float NearestDistanceSq = FMath::Square(MaxDistance);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsParagonGruxBossActor(Actor))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (DistanceSq <= NearestDistanceSq)
		{
			NearestDistanceSq = DistanceSq;
			NearestBoss = Actor;
		}
	}

	return NearestBoss;
}

ANovaClickMovePlayerController* UNovaParagonGruxBossComponent::ResolveNovaPlayerController() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		return Cast<ANovaClickMovePlayerController>(PC);
	}

	return nullptr;
}

bool UNovaParagonGruxBossComponent::RequestCounterWindow(const ENovaBossCounterType CounterType)
{
	ANovaClickMovePlayerController* NovaPC = ResolveNovaPlayerController();
	if (!NovaPC)
	{
		return false;
	}

	return NovaPC->OpenBossCounterWindow(CounterType, GetOwner());
}

void UNovaParagonGruxBossComponent::NotifyGruxCounterSucceeded(
	const ENovaBossCounterType CounterType,
	const ENovaVoiceCommand WeaponUsed)
{
	OnGruxCounterSucceeded.Broadcast(CounterType, WeaponUsed);
}
