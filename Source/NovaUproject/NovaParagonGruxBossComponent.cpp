#include "NovaParagonGruxBossComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NovaClickMovePlayerController.h"
#include "TimerManager.h"

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

	ApplyBossScale();

	if (bValidateParagonGruxMesh)
	{
		FString RejectReason;
		if (!ValidateGruxAssetOnOwner(RejectReason))
		{
			UE_LOG(LogTemp, Warning, TEXT("NovaParagonGruxBossComponent: %s"), *RejectReason);
		}
	}

	if (bAutoApplyDefaultCounterStagger)
	{
		OnGruxCounterSucceeded.AddDynamic(this, &UNovaParagonGruxBossComponent::HandleDefaultCounterReaction);
	}
}

void UNovaParagonGruxBossComponent::ApplyBossScale()
{
	if (!bApplyBossScaleOnBeginPlay)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || BossUniformScale <= 0.0f)
	{
		return;
	}

	const FVector CurrentScale = Owner->GetActorScale3D();
	Owner->SetActorScale3D(CurrentScale * BossUniformScale);
}

void UNovaParagonGruxBossComponent::ApplyCounterStaggerReaction(const float StaggerSeconds)
{
	ACharacter* BossCharacter = Cast<ACharacter>(GetOwner());
	if (!BossCharacter)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = BossCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.1f);
		}
	}

	if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = 0.0f;
	}

	// 그로기 타이머는 EnterGroggy에서 관리합니다. 비그로기 스턴만 여기서 복구합니다.
	if (!bIsGroggy)
	{
		const float Duration = StaggerSeconds > 0.0f ? StaggerSeconds : DefaultCounterStaggerSeconds;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StaggerTimerHandle);
			World->GetTimerManager().SetTimer(
				StaggerTimerHandle,
				this,
				&UNovaParagonGruxBossComponent::RestoreMovementAfterStagger,
				Duration,
				false);
		}
	}
}

void UNovaParagonGruxBossComponent::RestoreMovementAfterStagger()
{
	if (ACharacter* BossCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = SavedMaxWalkSpeed > 0.0f ? SavedMaxWalkSpeed : 600.0f;
		}
	}
}

void UNovaParagonGruxBossComponent::EnterGroggy(const float DurationSeconds)
{
	if (bIsGroggy)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StaggerTimerHandle);
		}
	}
	else
	{
		bIsGroggy = true;
		OnGroggyStarted.Broadcast();
	}

	const float Duration = DurationSeconds > 0.0f ? DurationSeconds : GroggyDurationSeconds;
	ApplyCounterStaggerReaction(Duration);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaggerTimerHandle);
		World->GetTimerManager().SetTimer(
			StaggerTimerHandle,
			this,
			&UNovaParagonGruxBossComponent::HandleGroggyTimerExpired,
			Duration,
			false);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.5f,
			FColor::Yellow,
			FString::Printf(
				TEXT("보스 그로기! (%.1fs, 피격 x%.1f)"),
				Duration,
				GroggyDamageMultiplier));
	}
}

void UNovaParagonGruxBossComponent::ExitGroggy()
{
	if (!bIsGroggy)
	{
		return;
	}

	bIsGroggy = false;
	RestoreMovementAfterStagger();
	OnGroggyEnded.Broadcast();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaggerTimerHandle);
	}
}

void UNovaParagonGruxBossComponent::HandleGroggyTimerExpired()
{
	ExitGroggy();
}

float UNovaParagonGruxBossComponent::GetGroggyDamageMultiplier() const
{
	return bIsGroggy ? GroggyDamageMultiplier : 1.0f;
}

bool UNovaParagonGruxBossComponent::IsActorGroggy(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	const UNovaParagonGruxBossComponent* GruxComponent = Actor->FindComponentByClass<UNovaParagonGruxBossComponent>();
	return GruxComponent && GruxComponent->IsGroggy();
}

float UNovaParagonGruxBossComponent::GetIncomingDamageMultiplierForActor(const AActor* Actor)
{
	if (!Actor)
	{
		return 1.0f;
	}

	const UNovaParagonGruxBossComponent* GruxComponent = Actor->FindComponentByClass<UNovaParagonGruxBossComponent>();
	if (!GruxComponent)
	{
		return 1.0f;
	}

	return GruxComponent->GetGroggyDamageMultiplier();
}

void UNovaParagonGruxBossComponent::HandleDefaultCounterReaction(
	const ENovaBossCounterType CounterType,
	const ENovaVoiceCommand WeaponUsed)
{
	EnterGroggy(GroggyDurationSeconds);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Emerald,
			FString::Printf(TEXT("Grux 상쇄 성공 → 그로기 (Pattern %d)"), static_cast<int32>(CounterType)));
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

bool UNovaParagonGruxBossComponent::RequestCounterWindow(
	const ENovaBossCounterType CounterType,
	const float CounterWindowSeconds)
{
	ANovaClickMovePlayerController* NovaPC = ResolveNovaPlayerController();
	if (!NovaPC)
	{
		return false;
	}

	return NovaPC->OpenBossCounterWindow(CounterType, GetOwner(), CounterWindowSeconds);
}

void UNovaParagonGruxBossComponent::NotifyGruxCounterSucceeded(
	const ENovaBossCounterType CounterType,
	const ENovaVoiceCommand WeaponUsed)
{
	OnGruxCounterSucceeded.Broadcast(CounterType, WeaponUsed);
}
