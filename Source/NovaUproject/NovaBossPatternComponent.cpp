#include "NovaBossPatternComponent.h"

#include "NovaFloatingHealthBarComponent.h"
#include "NovaParagonGruxBossComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace NovaBossPatternVisual
{
	static constexpr float BasicCylinderRadius = 50.0f;
	static constexpr float BasicCubeExtent = 50.0f;
	static constexpr float BasicSphereRadius = 50.0f;

	UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	UMaterialInstanceDynamic* CreateColoredMaterial(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!BaseMaterial)
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
		if (MaterialInstance)
		{
			MaterialInstance->SetVectorParameterValue(TEXT("Color"), Color);
		}

		return MaterialInstance;
	}

	FLinearColor GetPatternTelegraphBaseColor(const ENovaBossCounterType CounterType)
	{
		switch (CounterType)
		{
		case ENovaBossCounterType::Pattern_1:
			return FLinearColor(0.25f, 0.75f, 1.0f, 1.0f);
		case ENovaBossCounterType::Pattern_2:
			return FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
		case ENovaBossCounterType::Pattern_3:
			return FLinearColor(1.0f, 0.55f, 0.12f, 1.0f);
		case ENovaBossCounterType::Pattern_4:
			return FLinearColor(0.82f, 0.28f, 1.0f, 1.0f);
		default:
			return FLinearColor(1.0f, 0.85f, 0.1f, 1.0f);
		}
	}

	FLinearColor GetPatternExecuteBaseColor(const ENovaBossCounterType CounterType)
	{
		switch (CounterType)
		{
		case ENovaBossCounterType::Pattern_1:
			return FLinearColor(0.1f, 0.45f, 1.0f, 1.0f);
		case ENovaBossCounterType::Pattern_2:
			return FLinearColor(0.1f, 0.85f, 0.2f, 1.0f);
		case ENovaBossCounterType::Pattern_3:
			return FLinearColor(1.0f, 0.35f, 0.05f, 1.0f);
		case ENovaBossCounterType::Pattern_4:
			return FLinearColor(0.65f, 0.1f, 0.95f, 1.0f);
		default:
			return FLinearColor(1.0f, 0.15f, 0.05f, 1.0f);
		}
	}

	UStaticMeshComponent* CreateVisualMesh(AActor* Owner, const FName& Name, UStaticMesh* Mesh, UMaterialInstanceDynamic* Material)
	{
		if (!Owner || !Mesh)
		{
			return nullptr;
		}

		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Owner, UStaticMeshComponent::StaticClass(), Name);
		MeshComponent->SetStaticMesh(Mesh);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCastShadow(false);
		MeshComponent->SetGenerateOverlapEvents(false);
		if (Material)
		{
			MeshComponent->SetMaterial(0, Material);
		}

		Owner->AddInstanceComponent(MeshComponent);
		MeshComponent->RegisterComponent();
		MeshComponent->SetVisibility(false);
		return MeshComponent;
	}
}

UNovaBossPatternComponent::UNovaBossPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	InitializeDefaultGruxPatterns();
}

void UNovaBossPatternComponent::InitializeDefaultGruxPatterns()
{
	if (!AttackPatterns.IsEmpty())
	{
		return;
	}

	AttackPatterns = {
		{
			TEXT("Pattern_1_Dash"),
			ENovaBossCounterType::Pattern_1,
			DefaultTelegraphSeconds,
			DefaultPatternIntervalSeconds,
			28.0f,
			500.0f,
			0.0f,
			true,
			nullptr
		},
		{
			TEXT("Pattern_2_AoE45"),
			ENovaBossCounterType::Pattern_2,
			DefaultTelegraphSeconds,
			DefaultPatternIntervalSeconds,
			32.0f,
			420.0f,
			45.0f,
			true,
			nullptr
		},
		{
			TEXT("Pattern_3_Projectile"),
			ENovaBossCounterType::Pattern_3,
			DefaultTelegraphSeconds,
			DefaultPatternIntervalSeconds,
			22.0f,
			650.0f,
			0.0f,
			true,
			nullptr
		},
		{
			TEXT("Pattern_4_AoE360"),
			ENovaBossCounterType::Pattern_4,
			DefaultTelegraphSeconds,
			DefaultPatternIntervalSeconds,
			38.0f,
			320.0f,
			360.0f,
			true,
			nullptr
		}
	};
}

void UNovaBossPatternComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AttackPatterns.IsEmpty())
	{
		InitializeDefaultGruxPatterns();
	}

	EnsurePatternVisualMeshes();

	if (!EnsureGruxBossComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("NovaBossPatternComponent: Paragon Grux 보스가 아니거나 NovaParagonGruxBossComponent가 없습니다. 비활성화합니다."));
		return;
	}

	BindGruxCounterDelegate();
	EnsureBossHealthBar();

	if (bAutoStartOnBeginPlay)
	{
		StartBossAI();
	}
}

void UNovaBossPatternComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HidePatternVisuals();
	UnbindGruxCounterDelegate();
	Super::EndPlay(EndPlayReason);
}

void UNovaBossPatternComponent::StartBossAI()
{
	bFightActive = true;
	CurrentState = ENovaBossPatternState::Idle;
	ActivePatternIndex = INDEX_NONE;
	bCounteredThisPattern = false;
	StateTimer = 0.0f;
}

void UNovaBossPatternComponent::StopBossAI()
{
	bFightActive = false;
	CurrentState = ENovaBossPatternState::Idle;
	ActivePatternIndex = INDEX_NONE;
	bCounteredThisPattern = false;
	StateTimer = 0.0f;

	if (ACharacter* BossCharacter = GetOwnerCharacter())
	{
		if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
}

void UNovaBossPatternComponent::ForceNextPattern()
{
	if (!IsFightActive() || AttackPatterns.IsEmpty())
	{
		return;
	}

	if (CurrentState == ENovaBossPatternState::Telegraphing
		|| CurrentState == ENovaBossPatternState::Executing
		|| CurrentState == ENovaBossPatternState::Staggered)
	{
		return;
	}

	BeginTelegraph(NextPatternIndex);
}

ACharacter* UNovaBossPatternComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

APawn* UNovaBossPatternComponent::GetPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

float UNovaBossPatternComponent::GetDistanceToPlayer() const
{
	const APawn* PlayerPawn = GetPlayerPawn();
	const ACharacter* BossCharacter = GetOwnerCharacter();
	if (!PlayerPawn || !BossCharacter)
	{
		return TNumericLimits<float>::Max();
	}

	return FVector::Dist(BossCharacter->GetActorLocation(), PlayerPawn->GetActorLocation());
}

bool UNovaBossPatternComponent::IsPlayerInAggroRange() const
{
	return GetDistanceToPlayer() <= AggroRadius;
}

bool UNovaBossPatternComponent::IsPlayerInAttackRange() const
{
	return GetDistanceToPlayer() <= AttackRadius;
}

const FNovaBossAttackPattern* UNovaBossPatternComponent::GetActivePattern() const
{
	if (!AttackPatterns.IsValidIndex(ActivePatternIndex))
	{
		return nullptr;
	}

	return &AttackPatterns[ActivePatternIndex];
}

UNovaParagonGruxBossComponent* UNovaBossPatternComponent::ResolveGruxBossComponent() const
{
	if (GruxBossComponent)
	{
		return GruxBossComponent;
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<UNovaParagonGruxBossComponent>() : nullptr;
}

bool UNovaBossPatternComponent::EnsureGruxBossComponent()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const USkeletalMeshComponent* MeshComp = Owner->FindComponentByClass<USkeletalMeshComponent>();
	const bool bHasGruxMesh = MeshComp && MeshComp->GetSkeletalMeshAsset()
		&& MeshComp->GetSkeletalMeshAsset()->GetPathName().Contains(TEXT("ParagonGrux"), ESearchCase::IgnoreCase);

	if (!UNovaParagonGruxBossComponent::IsParagonGruxBossActor(Owner) && !bHasGruxMesh)
	{
		return false;
	}

	GruxBossComponent = Owner->FindComponentByClass<UNovaParagonGruxBossComponent>();
	if (!GruxBossComponent)
	{
		GruxBossComponent = NewObject<UNovaParagonGruxBossComponent>(Owner, TEXT("NovaParagonGruxBoss"));
		Owner->AddInstanceComponent(GruxBossComponent);
		GruxBossComponent->RegisterComponent();
		UE_LOG(LogTemp, Warning, TEXT("NovaBossPatternComponent: NovaParagonGruxBossComponent를 런타임에 추가했습니다. Grux BP에 컴포넌트를 붙여 주세요."));
	}

	bGruxIntegrationReady = GruxBossComponent != nullptr;
	return bGruxIntegrationReady;
}

void UNovaBossPatternComponent::BindGruxCounterDelegate()
{
	UNovaParagonGruxBossComponent* GruxComponent = ResolveGruxBossComponent();
	if (!GruxComponent)
	{
		return;
	}

	UnbindGruxCounterDelegate();
	GruxComponent->OnGruxCounterSucceeded.AddDynamic(this, &UNovaBossPatternComponent::HandleGruxCounterSucceeded);
}

void UNovaBossPatternComponent::UnbindGruxCounterDelegate()
{
	if (UNovaParagonGruxBossComponent* GruxComponent = ResolveGruxBossComponent())
	{
		GruxComponent->OnGruxCounterSucceeded.RemoveDynamic(this, &UNovaBossPatternComponent::HandleGruxCounterSucceeded);
	}
}

void UNovaBossPatternComponent::NotifyPatternTelegraph(const ENovaBossCounterType CounterType)
{
	if (UNovaParagonGruxBossComponent* GruxComponent = ResolveGruxBossComponent())
	{
		float CounterWindowSeconds = DefaultTelegraphSeconds;
		if (const FNovaBossAttackPattern* Pattern = GetActivePattern())
		{
			CounterWindowSeconds = Pattern->TelegraphSeconds;
		}

		GruxComponent->RequestCounterWindow(CounterType, CounterWindowSeconds);
	}
}

void UNovaBossPatternComponent::EnsureBossHealthBar()
{
	if (!bAutoAddHealthBar)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BossHealthBarComponent = Owner->FindComponentByClass<UNovaFloatingHealthBarComponent>();
	if (!BossHealthBarComponent)
	{
		BossHealthBarComponent = NewObject<UNovaFloatingHealthBarComponent>(Owner, TEXT("NovaBossHealthBar"));
		Owner->AddInstanceComponent(BossHealthBarComponent);
		BossHealthBarComponent->RegisterComponent();
	}

	BossHealthBarComponent->ApplyBossHealthBarPreset(BossMaxHealth);
	BossHealthBarComponent->ActivateHealthBar();
}

void UNovaBossPatternComponent::HandleGruxCounterSucceeded(ENovaBossCounterType CounterType, ENovaVoiceCommand Command)
{
	const FNovaBossAttackPattern* ActivePattern = GetActivePattern();
	if (!ActivePattern || ActivePattern->CounterType != CounterType)
	{
		return;
	}

	if (CurrentState != ENovaBossPatternState::Telegraphing && CurrentState != ENovaBossPatternState::Executing)
	{
		return;
	}

	HidePatternVisuals();
	CancelPatternExecution();
	bCounteredThisPattern = true;
	EnterStaggered();
	OnPatternCountered.Broadcast(CounterType);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Emerald,
			FString::Printf(TEXT("패턴 상쇄 성공 → 그로기 (Pattern %d)"), static_cast<int32>(CounterType))
		);
	}
}

void UNovaBossPatternComponent::FacePlayer(const float DeltaTime)
{
	ACharacter* BossCharacter = GetOwnerCharacter();
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!BossCharacter || !PlayerPawn)
	{
		return;
	}

	FVector PlayerFaceLocation = PlayerPawn->GetActorLocation();
	if (const ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
	{
		PlayerFaceLocation.Z += PlayerCharacter->BaseEyeHeight * 0.85f;
	}

	FVector ToPlayer = PlayerFaceLocation - BossCharacter->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (ToPlayer.IsNearlyZero())
	{
		return;
	}

	const float TargetYaw = FMath::UnwindDegrees(ToPlayer.Rotation().Yaw + BossFacingYawOffset);
	const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);
	if (DeltaTime <= 0.0f)
	{
		BossCharacter->SetActorRotation(TargetRotation);
		return;
	}

	const FRotator NewRotation = FMath::RInterpConstantTo(
		BossCharacter->GetActorRotation(),
		TargetRotation,
		DeltaTime,
		FacePlayerRotationSpeed);
	BossCharacter->SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
}

void UNovaBossPatternComponent::TickFacePlayer(const float DeltaTime)
{
	if (!bAlwaysFacePlayer || UNovaParagonGruxBossComponent::IsActorGroggy(GetOwner()))
	{
		return;
	}

	if (!IsPlayerInAggroRange())
	{
		return;
	}

	if (CurrentState == ENovaBossPatternState::Executing)
	{
		const FNovaBossAttackPattern* Pattern = GetActivePattern();
		if (Pattern && Pattern->CounterType == ENovaBossCounterType::Pattern_1)
		{
			return;
		}
	}

	FacePlayer(DeltaTime);
}

void UNovaBossPatternComponent::AdvancePatternIndex()
{
	if (AttackPatterns.IsEmpty())
	{
		return;
	}

	if (bRotatePatternsInOrder)
	{
		NextPatternIndex = (NextPatternIndex + 1) % AttackPatterns.Num();
	}
	else
	{
		int32 RandomIndex = FMath::RandRange(0, AttackPatterns.Num() - 1);
		if (AttackPatterns.Num() > 1)
		{
			while (RandomIndex == ActivePatternIndex)
			{
				RandomIndex = FMath::RandRange(0, AttackPatterns.Num() - 1);
			}
		}
		NextPatternIndex = RandomIndex;
	}
}

void UNovaBossPatternComponent::BeginTelegraph(const int32 PatternIndex)
{
	if (!AttackPatterns.IsValidIndex(PatternIndex))
	{
		return;
	}

	ActivePatternIndex = PatternIndex;
	bCounteredThisPattern = false;
	bExecuteVisualActive = false;
	ExecuteVisualTimer = 0.0f;
	TelegraphVisualPulse = 0.0f;
	CurrentState = ENovaBossPatternState::Telegraphing;
	StateTimer = AttackPatterns[PatternIndex].TelegraphSeconds;

	FacePlayer();

	const FNovaBossAttackPattern& Pattern = AttackPatterns[PatternIndex];
	if (Pattern.AttackMontage)
	{
		if (ACharacter* BossCharacter = GetOwnerCharacter())
		{
			if (UAnimInstance* AnimInstance = BossCharacter->GetMesh() ? BossCharacter->GetMesh()->GetAnimInstance() : nullptr)
			{
				AnimInstance->Montage_Play(Pattern.AttackMontage);
			}
		}
	}

	if (Pattern.bOpensCounterWindow && Pattern.CounterType != ENovaBossCounterType::None)
	{
		NotifyPatternTelegraph(Pattern.CounterType);
	}

	OnPatternTelegraphStarted.Broadcast(Pattern.CounterType, PatternIndex);
	UpdateTelegraphVisual(0.0f);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Orange,
			FString::Printf(TEXT("Grux telegraph: %s"), *Pattern.PatternId.ToString())
		);
	}
}

void UNovaBossPatternComponent::ExecuteActivePattern()
{
	if (!GetActivePattern() || bCounteredThisPattern)
	{
		return;
	}

	BeginPatternExecution();
}

void UNovaBossPatternComponent::BeginPatternExecution()
{
	const FNovaBossAttackPattern* Pattern = GetActivePattern();
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (!Pattern || !BossCharacter || bCounteredThisPattern)
	{
		return;
	}

	FacePlayer();
	CurrentState = ENovaBossPatternState::Executing;
	ExecutionTimer = 0.0f;
	ExecutionSubPhase = 0;
	bExecutionDamageApplied = false;
	ActiveRockProjectiles.Reset();

	switch (Pattern->CounterType)
	{
	case ENovaBossCounterType::Pattern_1:
		ExecutionDuration = ChargeWindUpSeconds + ChargeExecuteSeconds;
		ExecutionSubPhase = 0;
		ChargeStartLocation = BossCharacter->GetActorLocation();
		ChargeDirection = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
		if (ChargeDirection.IsNearlyZero())
		{
			ChargeDirection = FVector::ForwardVector;
		}
		if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
		break;
	case ENovaBossCounterType::Pattern_2:
		ExecutionDuration = ForwardStompUpSeconds + ForwardStompDownSeconds;
		BeginSlamJump(ForwardStompHeight);
		break;
	case ENovaBossCounterType::Pattern_3:
		ExecutionDuration = RockFlightSeconds;
		LaunchRockProjectiles(*Pattern);
		break;
	case ENovaBossCounterType::Pattern_4:
		ExecutionDuration = JumpSlamUpSeconds + JumpSlamDownSeconds;
		BeginSlamJump(JumpSlamHeight);
		break;
	default:
		ExecutionDuration = ExecuteVisualSeconds;
		break;
	}

	PlayExecuteVisual(*Pattern);
}

void UNovaBossPatternComponent::CancelPatternExecution()
{
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (BossCharacter)
	{
		if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
		{
			Movement->GravityScale = 1.0f;
			Movement->StopMovementImmediately();
		}
	}

	ActiveRockProjectiles.Reset();
	HidePatternVisuals();

	ExecutionTimer = 0.0f;
	ExecutionDuration = 0.0f;
	ExecutionSubPhase = 0;
	bExecutionDamageApplied = false;
}

void UNovaBossPatternComponent::FinishPatternExecution()
{
	const FNovaBossAttackPattern* Pattern = GetActivePattern();
	if (!Pattern)
	{
		return;
	}

	ACharacter* BossCharacter = GetOwnerCharacter();
	if (BossCharacter)
	{
		if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
		{
			Movement->GravityScale = 1.0f;
		}
	}

	ActiveRockProjectiles.Reset();
	for (UStaticMeshComponent* RockMesh : RockProjectileMeshes)
	{
		if (RockMesh)
		{
			RockMesh->SetVisibility(false);
		}
	}
	if (WeaponSwingVisualMesh)
	{
		WeaponSwingVisualMesh->SetVisibility(false);
	}
	if (AimVisualMesh)
	{
		AimVisualMesh->SetVisibility(false);
	}

	if (Pattern->CounterType == ENovaBossCounterType::Pattern_4
		|| Pattern->CounterType == ENovaBossCounterType::Pattern_2)
	{
		bExecuteVisualActive = true;
		ExecuteVisualTimer = ExecuteVisualSeconds;
	}
	else
	{
		if (AreaVisualMesh)
		{
			AreaVisualMesh->SetVisibility(false);
		}
		if (ImpactVisualMesh)
		{
			ImpactVisualMesh->SetVisibility(false);
		}
	}

	ExecutionTimer = 0.0f;
	ExecutionDuration = 0.0f;
	ExecutionSubPhase = 0;

	OnPatternExecuted.Broadcast(Pattern->CounterType, ActivePatternIndex);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Red,
			FString::Printf(TEXT("Grux pattern: %s"), *Pattern->PatternId.ToString()));
	}

	CurrentState = ENovaBossPatternState::Recovering;
	StateTimer = Pattern->RecoverySeconds;
	AdvancePatternIndex();
}

bool UNovaBossPatternComponent::IsPlayerInPatternArc(
	const FNovaBossAttackPattern& Pattern,
	const FVector& Origin,
	const FVector& Forward) const
{
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		return false;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const float Distance = FVector::Dist2D(Origin, PlayerLocation);
	if (Distance > Pattern.AttackRadius)
	{
		return false;
	}

	if (Pattern.AttackArcDegrees <= 0.0f || Pattern.AttackArcDegrees >= 360.0f)
	{
		return true;
	}

	const FVector ToPlayer = (PlayerLocation - Origin).GetSafeNormal2D();
	const FVector Forward2D = Forward.GetSafeNormal2D();
	const float Dot = FVector::DotProduct(Forward2D, ToPlayer);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	return AngleDeg <= (Pattern.AttackArcDegrees * 0.5f);
}

bool UNovaBossPatternComponent::IsPlayerInChargeLane(
	const FNovaBossAttackPattern& Pattern,
	const FVector& Start,
	const FVector& End) const
{
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!PlayerPawn)
	{
		return false;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector Segment = End - Start;
	const float SegmentLengthSq = Segment.SizeSquared2D();
	if (SegmentLengthSq <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float T = FMath::Clamp(
		FVector::DotProduct(PlayerLocation - Start, Segment) / SegmentLengthSq,
		0.0f,
		1.0f);
	const FVector ClosestPoint = Start + Segment * T;
	const float LaneHalfWidth = 160.0f;
	return FVector::Dist2D(ClosestPoint, PlayerLocation) <= LaneHalfWidth
		&& FVector::Dist2D(Start, ClosestPoint) <= Pattern.AttackRadius;
}

void UNovaBossPatternComponent::TryApplyPatternDamage(const FNovaBossAttackPattern& Pattern)
{
	if (bExecutionDamageApplied)
	{
		return;
	}

	APawn* PlayerPawn = GetPlayerPawn();
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (!PlayerPawn || !BossCharacter)
	{
		return;
	}

	const FVector BossLocation = BossCharacter->GetActorLocation();
	const FVector Forward = BossCharacter->GetActorForwardVector();
	if (!IsPlayerInPatternArc(Pattern, BossLocation, Forward))
	{
		return;
	}

	bExecutionDamageApplied = true;
	UGameplayStatics::ApplyDamage(
		PlayerPawn,
		Pattern.Damage,
		BossCharacter->GetController(),
		BossCharacter,
		nullptr);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Red,
			FString::Printf(TEXT("Grux hit: %s (%.0f dmg)"), *Pattern.PatternId.ToString(), Pattern.Damage));
	}
}

void UNovaBossPatternComponent::BeginSlamJump(const float SlamHeight)
{
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (!BossCharacter)
	{
		return;
	}

	ActiveSlamHeight = SlamHeight;
	JumpStartGroundZ = BossCharacter->GetActorLocation().Z;
	JumpSlamOrigin = BossCharacter->GetActorLocation();
	ExecutionSubPhase = 0;

	if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->GravityScale = 0.0f;
	}

	BossCharacter->LaunchCharacter(
		FVector(0.0f, 0.0f, FMath::Sqrt(2.0f * 980.0f * ActiveSlamHeight)),
		false,
		true);
}

void UNovaBossPatternComponent::LaunchRockProjectiles(const FNovaBossAttackPattern& Pattern)
{
	const ACharacter* BossCharacter = GetOwnerCharacter();
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!BossCharacter)
	{
		return;
	}

	const FVector BossLocation = BossCharacter->GetActorLocation();
	const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
	FVector TargetLocation = BossLocation + Forward * Pattern.AttackRadius;
	if (PlayerPawn)
	{
		TargetLocation = PlayerPawn->GetActorLocation();
	}

	const int32 RockCount = FMath::Clamp(RockProjectileCount, 1, RockProjectileMeshes.Num());
	const FVector AimDirection = (TargetLocation - BossLocation).GetSafeNormal2D();
	const FVector AimRight = FVector::CrossProduct(FVector::UpVector, AimDirection).GetSafeNormal();

	for (int32 Index = 0; Index < RockCount; ++Index)
	{
		FRockProjectileVisual Rock;
		const float Spread = (Index - (RockCount - 1) * 0.5f) * 90.0f;
		const FVector SpreadOffset = AimRight * Spread;
		Rock.Start = BossLocation + FVector(0.0f, 0.0f, 130.0f) + SpreadOffset * 0.15f;
		Rock.End = TargetLocation + SpreadOffset + FVector(0.0f, 0.0f, 30.0f);
		Rock.FlightTime = RockFlightSeconds;
		Rock.Scale = FVector(
			FMath::FRandRange(0.22f, 0.38f),
			FMath::FRandRange(0.18f, 0.32f),
			FMath::FRandRange(0.16f, 0.28f));
		Rock.SpinRate = FRotator(
			FMath::FRandRange(240.0f, 520.0f),
			FMath::FRandRange(240.0f, 520.0f),
			FMath::FRandRange(240.0f, 520.0f));
		ActiveRockProjectiles.Add(Rock);

		if (RockProjectileMeshes.IsValidIndex(Index) && RockProjectileMeshes[Index])
		{
			UStaticMeshComponent* RockMesh = RockProjectileMeshes[Index];
			RockMesh->SetMaterial(0, GetPatternExecuteMaterial(Pattern.CounterType));
			RockMesh->SetWorldLocation(Rock.Start);
			RockMesh->SetWorldScale3D(Rock.Scale);
			RockMesh->SetVisibility(true);
		}
	}
}

void UNovaBossPatternComponent::UpdateChargeExecution(const float DeltaTime, const FNovaBossAttackPattern& Pattern)
{
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (!BossCharacter)
	{
		return;
	}

	const float LaneLength = Pattern.AttackRadius;
	const FVector TargetLocation = ChargeStartLocation + ChargeDirection * LaneLength;

	if (ExecutionSubPhase == 0)
	{
		const float WindUpAlpha = FMath::Clamp(ExecutionTimer / FMath::Max(ChargeWindUpSeconds, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		if (AimVisualMesh)
		{
			AimVisualMesh->SetMaterial(0, GetPatternExecuteMaterial(Pattern.CounterType));
			const FVector LaneCenter = GetGroundLocationAt(ChargeStartLocation + ChargeDirection * (LaneLength * 0.45f));
			AimVisualMesh->SetWorldLocation(LaneCenter);
			AimVisualMesh->SetWorldRotation(ChargeDirection.Rotation());
			AimVisualMesh->SetWorldScale3D(FVector(
				LaneLength / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
				FMath::Lerp(0.15f, 0.5f, WindUpAlpha),
				0.07f));
			AimVisualMesh->SetVisibility(true);
		}

		if (ExecutionTimer >= ChargeWindUpSeconds)
		{
			ExecutionSubPhase = 1;
			ChargeStartLocation = BossCharacter->GetActorLocation();
		}
		return;
	}

	const float ChargeElapsed = ExecutionTimer - ChargeWindUpSeconds;
	const float ChargeAlpha = FMath::Clamp(
		ChargeElapsed / FMath::Max(ChargeExecuteSeconds, KINDA_SMALL_NUMBER),
		0.0f,
		1.0f);
	const FVector NewLocation = FMath::Lerp(ChargeStartLocation, TargetLocation, ChargeAlpha);
	BossCharacter->SetActorLocation(NewLocation, true);

	if (AimVisualMesh)
	{
		AimVisualMesh->SetMaterial(0, GetPatternExecuteMaterial(Pattern.CounterType));
		const FVector LaneCenter = GetGroundLocationAt((ChargeStartLocation + NewLocation) * 0.5f);
		AimVisualMesh->SetWorldLocation(LaneCenter);
		AimVisualMesh->SetWorldRotation(ChargeDirection.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(
			LaneLength / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.5f,
			0.07f));
		AimVisualMesh->SetVisibility(true);
	}

	if (IsPlayerInChargeLane(Pattern, ChargeStartLocation, NewLocation))
	{
		APawn* PlayerPawn = GetPlayerPawn();
		if (PlayerPawn && !bExecutionDamageApplied)
		{
			bExecutionDamageApplied = true;
			UGameplayStatics::ApplyDamage(
				PlayerPawn,
				Pattern.Damage,
				BossCharacter->GetController(),
				BossCharacter,
				nullptr);
		}
	}
}

void UNovaBossPatternComponent::UpdateRockProjectileExecution(const float DeltaTime, const FNovaBossAttackPattern& Pattern)
{
	APawn* PlayerPawn = GetPlayerPawn();
	ACharacter* BossCharacter = GetOwnerCharacter();
	bool bAllLanded = true;

	for (int32 Index = 0; Index < ActiveRockProjectiles.Num(); ++Index)
	{
		FRockProjectileVisual& Rock = ActiveRockProjectiles[Index];
		Rock.Elapsed += DeltaTime;
		const float Alpha = FMath::Clamp(Rock.Elapsed / FMath::Max(Rock.FlightTime, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		const FVector BaseLocation = FMath::Lerp(Rock.Start, Rock.End, Alpha);
		const float Arc = FMath::Sin(Alpha * PI) * RockArcHeight;
		const FVector RockLocation = BaseLocation + FVector(0.0f, 0.0f, Arc);

		if (RockProjectileMeshes.IsValidIndex(Index) && RockProjectileMeshes[Index])
		{
			UStaticMeshComponent* RockMesh = RockProjectileMeshes[Index];
			RockMesh->SetWorldLocation(RockLocation);
			RockMesh->AddWorldRotation(Rock.SpinRate * DeltaTime);
			RockMesh->SetVisibility(true);
		}

		if (Alpha < 1.0f)
		{
			bAllLanded = false;
		}
		else if (!Rock.bHitApplied && PlayerPawn && BossCharacter)
		{
			const float HitRadius = 140.0f;
			if (FVector::Dist2D(Rock.End, PlayerPawn->GetActorLocation()) <= HitRadius)
			{
				Rock.bHitApplied = true;
				UGameplayStatics::ApplyDamage(
					PlayerPawn,
					Pattern.Damage,
					BossCharacter->GetController(),
					BossCharacter,
					nullptr);
			}
		}
	}

	if (bAllLanded && ExecutionTimer >= ExecutionDuration)
	{
		for (UStaticMeshComponent* RockMesh : RockProjectileMeshes)
		{
			if (RockMesh)
			{
				RockMesh->SetVisibility(false);
			}
		}
	}
}

void UNovaBossPatternComponent::UpdateJumpSlamExecution(const float DeltaTime, const FNovaBossAttackPattern& Pattern)
{
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (!BossCharacter)
	{
		return;
	}

	const float SlamUpSeconds = Pattern.CounterType == ENovaBossCounterType::Pattern_4
		? JumpSlamUpSeconds
		: ForwardStompUpSeconds;
	const float SlamDownSeconds = Pattern.CounterType == ENovaBossCounterType::Pattern_4
		? JumpSlamDownSeconds
		: ForwardStompDownSeconds;

	if (ExecutionSubPhase == 0)
	{
		if (ExecutionTimer >= SlamUpSeconds)
		{
			ExecutionSubPhase = 1;
			if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
			{
				Movement->GravityScale = 2.5f;
				Movement->Velocity = FVector(0.0f, 0.0f, -1600.0f);
			}
		}
	}
	else if (ExecutionSubPhase == 1)
	{
		const float DownAlpha = (ExecutionTimer - SlamUpSeconds) / FMath::Max(SlamDownSeconds, KINDA_SMALL_NUMBER);
		const float CurrentZ = FMath::Lerp(
			JumpStartGroundZ + ActiveSlamHeight,
			JumpStartGroundZ,
			FMath::Clamp(DownAlpha, 0.0f, 1.0f));
		FVector Location = BossCharacter->GetActorLocation();
		Location.Z = CurrentZ;
		BossCharacter->SetActorLocation(Location, true);

		if (DownAlpha >= 0.95f)
		{
			ExecutionSubPhase = 2;
			if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
			{
				Movement->GravityScale = 1.0f;
				Movement->StopMovementImmediately();
			}

			FVector GroundLocation = JumpSlamOrigin;
			GroundLocation.Z = JumpStartGroundZ;
			BossCharacter->SetActorLocation(GroundLocation, true);

			const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
			const FVector GroundHit = GetGroundLocationAt(GroundLocation);
			const bool bFullCircle = Pattern.AttackArcDegrees <= 0.0f || Pattern.AttackArcDegrees >= 360.0f;

			if (AreaVisualMesh)
			{
				if (bFullCircle)
				{
					AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
					AreaVisualMesh->SetWorldLocation(GroundHit);
					AreaVisualMesh->SetWorldRotation(FRotator::ZeroRotator);
					const float RadiusScale = Pattern.AttackRadius / NovaBossPatternVisual::BasicCylinderRadius;
					AreaVisualMesh->SetWorldScale3D(FVector(RadiusScale * 1.2f, RadiusScale * 1.2f, 0.14f));
				}
				else
				{
					const float HalfAngleRad = FMath::DegreesToRadians(Pattern.AttackArcDegrees * 0.5f);
					const float WedgeWidth = 2.0f * Pattern.AttackRadius * FMath::Tan(HalfAngleRad);
					const FVector WedgeCenter = GroundHit + Forward * (Pattern.AttackRadius * 0.5f);
					AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
					AreaVisualMesh->SetWorldLocation(WedgeCenter);
					AreaVisualMesh->SetWorldRotation(Forward.Rotation());
					AreaVisualMesh->SetWorldScale3D(FVector(
						Pattern.AttackRadius / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
						WedgeWidth / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
						0.12f));
				}

				AreaVisualMesh->SetMaterial(0, GetPatternExecuteMaterial(Pattern.CounterType));
				AreaVisualMesh->SetVisibility(true);
			}

			if (ImpactVisualMesh)
			{
				ImpactVisualMesh->SetMaterial(0, GetPatternExecuteMaterial(Pattern.CounterType));
				ImpactVisualMesh->SetWorldLocation(GroundHit + FVector(0.0f, 0.0f, 40.0f));
				ImpactVisualMesh->SetWorldScale3D(FVector(bFullCircle ? 1.4f : 1.0f, bFullCircle ? 1.4f : 0.8f, 0.35f));
				ImpactVisualMesh->SetVisibility(true);
			}

			TryApplyPatternDamage(Pattern);
		}
	}
}

void UNovaBossPatternComponent::EnterStaggered()
{
	CurrentState = ENovaBossPatternState::Staggered;

	float GroggyDuration = StaggerDuration;
	if (const UNovaParagonGruxBossComponent* GruxComponent = ResolveGruxBossComponent())
	{
		GroggyDuration = GruxComponent->GroggyDurationSeconds;
	}
	StateTimer = GroggyDuration;

	AdvancePatternIndex();

	if (ACharacter* BossCharacter = GetOwnerCharacter())
	{
		if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
}

void UNovaBossPatternComponent::UpdateIdle()
{
	if (UNovaParagonGruxBossComponent::IsActorGroggy(GetOwner()))
	{
		return;
	}

	if (!IsPlayerInAggroRange())
	{
		return;
	}

	if (IsPlayerInAttackRange())
	{
		BeginTelegraph(NextPatternIndex);
		return;
	}

	CurrentState = ENovaBossPatternState::Chasing;
}

void UNovaBossPatternComponent::UpdateChasing(const float DeltaTime)
{
	if (UNovaParagonGruxBossComponent::IsActorGroggy(GetOwner()))
	{
		if (ACharacter* BossCharacter = GetOwnerCharacter())
		{
			if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}
		return;
	}

	if (!IsPlayerInAggroRange())
	{
		CurrentState = ENovaBossPatternState::Idle;
		return;
	}

	if (IsPlayerInAttackRange())
	{
		if (ACharacter* BossCharacter = GetOwnerCharacter())
		{
			if (UCharacterMovementComponent* Movement = BossCharacter->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}

		BeginTelegraph(NextPatternIndex);
		return;
	}

	ACharacter* BossCharacter = GetOwnerCharacter();
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!BossCharacter || !PlayerPawn)
	{
		return;
	}

	FVector Direction = PlayerPawn->GetActorLocation() - BossCharacter->GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	Direction.Normalize();
	BossCharacter->AddMovementInput(Direction, ChaseMoveSpeedMultiplier);
}

void UNovaBossPatternComponent::UpdateTelegraphing(const float DeltaTime)
{
	UpdateTelegraphVisual(DeltaTime);

	StateTimer -= DeltaTime;
	if (StateTimer <= 0.0f && !bCounteredThisPattern)
	{
		ExecuteActivePattern();
	}
}

void UNovaBossPatternComponent::UpdateExecuting(const float DeltaTime)
{
	if (bCounteredThisPattern)
	{
		return;
	}

	const FNovaBossAttackPattern* Pattern = GetActivePattern();
	if (!Pattern)
	{
		FinishPatternExecution();
		return;
	}

	ExecutionTimer += DeltaTime;

	switch (Pattern->CounterType)
	{
	case ENovaBossCounterType::Pattern_1:
		UpdateChargeExecution(DeltaTime, *Pattern);
		break;
	case ENovaBossCounterType::Pattern_2:
		UpdateJumpSlamExecution(DeltaTime, *Pattern);
		break;
	case ENovaBossCounterType::Pattern_3:
		UpdateRockProjectileExecution(DeltaTime, *Pattern);
		break;
	case ENovaBossCounterType::Pattern_4:
		UpdateJumpSlamExecution(DeltaTime, *Pattern);
		break;
	default:
		break;
	}

	if (ExecutionTimer >= ExecutionDuration)
	{
		FinishPatternExecution();
	}
}

void UNovaBossPatternComponent::UpdateRecovering(const float DeltaTime)
{
	UpdateExecuteVisual(DeltaTime);

	StateTimer -= DeltaTime;
	if (StateTimer <= 0.0f)
	{
		HidePatternVisuals();
		CurrentState = ENovaBossPatternState::Idle;
	}
}

void UNovaBossPatternComponent::UpdateStaggered(const float DeltaTime)
{
	StateTimer -= DeltaTime;
	if (StateTimer <= 0.0f)
	{
		bCounteredThisPattern = false;
		CurrentState = ENovaBossPatternState::Idle;
	}
}

void UNovaBossPatternComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsFightActive())
	{
		return;
	}

	TickFacePlayer(DeltaTime);

	if (bGruxIntegrationReady)
	{
		BindGruxCounterDelegate();
	}

	switch (CurrentState)
	{
	case ENovaBossPatternState::Idle:
		UpdateIdle();
		break;
	case ENovaBossPatternState::Chasing:
		UpdateChasing(DeltaTime);
		break;
	case ENovaBossPatternState::Telegraphing:
		UpdateTelegraphing(DeltaTime);
		break;
	case ENovaBossPatternState::Executing:
		UpdateExecuting(DeltaTime);
		break;
	case ENovaBossPatternState::Recovering:
		UpdateRecovering(DeltaTime);
		break;
	case ENovaBossPatternState::Staggered:
		UpdateStaggered(DeltaTime);
		break;
	default:
		break;
	}

	if (CurrentState != ENovaBossPatternState::Telegraphing
		&& CurrentState != ENovaBossPatternState::Recovering
		&& !bExecuteVisualActive)
	{
		HidePatternVisuals();
	}
}

void UNovaBossPatternComponent::EnsurePatternVisualMeshes()
{
	if (!bShowPatternVisuals || bPatternVisualMeshesReady)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UStaticMesh* CylinderMesh = NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* CubeMesh = NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereMesh = NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!CylinderMesh || !CubeMesh || !SphereMesh)
	{
		return;
	}

	TelegraphVisualMaterial = NovaBossPatternVisual::CreateColoredMaterial(this, FLinearColor(1.0f, 0.85f, 0.1f, 1.0f));
	ExecuteVisualMaterial = NovaBossPatternVisual::CreateColoredMaterial(this, FLinearColor(1.0f, 0.15f, 0.05f, 1.0f));
	RockVisualMaterial = NovaBossPatternVisual::CreateColoredMaterial(this, FLinearColor(0.34f, 0.28f, 0.22f, 1.0f));

	AreaVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternAreaVisual"), CylinderMesh, TelegraphVisualMaterial);
	AimVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternAimVisual"), CubeMesh, TelegraphVisualMaterial);
	ImpactVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternImpactVisual"), CubeMesh, ExecuteVisualMaterial);
	WeaponSwingVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternWeaponVisual"), CubeMesh, ExecuteVisualMaterial);

	RockProjectileMeshes.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FName RockName = *FString::Printf(TEXT("BossPatternRockVisual_%d"), Index);
		if (UStaticMeshComponent* RockMesh = NovaBossPatternVisual::CreateVisualMesh(
			Owner,
			RockName,
			CubeMesh,
			RockVisualMaterial ? RockVisualMaterial : ExecuteVisualMaterial))
		{
			RockProjectileMeshes.Add(RockMesh);
		}
	}

	bPatternVisualMeshesReady = AreaVisualMesh != nullptr && AimVisualMesh != nullptr && ImpactVisualMesh != nullptr;
	EnsurePatternColorMaterials();
}

void UNovaBossPatternComponent::EnsurePatternColorMaterials()
{
	const ENovaBossCounterType PatternTypes[] = {
		ENovaBossCounterType::Pattern_1,
		ENovaBossCounterType::Pattern_2,
		ENovaBossCounterType::Pattern_3,
		ENovaBossCounterType::Pattern_4
	};

	for (const ENovaBossCounterType CounterType : PatternTypes)
	{
		if (!PatternTelegraphMaterials.Contains(CounterType))
		{
			PatternTelegraphMaterials.Add(
				CounterType,
				NovaBossPatternVisual::CreateColoredMaterial(
					this,
					NovaBossPatternVisual::GetPatternTelegraphBaseColor(CounterType)));
		}

		if (!PatternExecuteMaterials.Contains(CounterType))
		{
			PatternExecuteMaterials.Add(
				CounterType,
				NovaBossPatternVisual::CreateColoredMaterial(
					this,
					NovaBossPatternVisual::GetPatternExecuteBaseColor(CounterType)));
		}
	}
}

UMaterialInstanceDynamic* UNovaBossPatternComponent::GetPatternTelegraphMaterial(const ENovaBossCounterType CounterType) const
{
	if (const TObjectPtr<UMaterialInstanceDynamic>* Found = PatternTelegraphMaterials.Find(CounterType))
	{
		return Found->Get();
	}

	return TelegraphVisualMaterial;
}

UMaterialInstanceDynamic* UNovaBossPatternComponent::GetPatternExecuteMaterial(const ENovaBossCounterType CounterType) const
{
	if (const TObjectPtr<UMaterialInstanceDynamic>* Found = PatternExecuteMaterials.Find(CounterType))
	{
		return Found->Get();
	}

	return ExecuteVisualMaterial;
}

void UNovaBossPatternComponent::PulsePatternMaterial(
	UMaterialInstanceDynamic* Material,
	const FLinearColor& BaseColor,
	const float PulseAlpha) const
{
	if (!Material)
	{
		return;
	}

	const float PulseColor = 0.75f + 0.25f * FMath::Sin(PulseAlpha);
	Material->SetVectorParameterValue(TEXT("Color"), BaseColor * PulseColor);
}

void UNovaBossPatternComponent::HidePatternVisuals()
{
	if (AreaVisualMesh)
	{
		AreaVisualMesh->SetVisibility(false);
	}
	if (AimVisualMesh)
	{
		AimVisualMesh->SetVisibility(false);
	}
	if (ImpactVisualMesh)
	{
		ImpactVisualMesh->SetVisibility(false);
	}
	if (WeaponSwingVisualMesh)
	{
		WeaponSwingVisualMesh->SetVisibility(false);
	}
	for (UStaticMeshComponent* RockMesh : RockProjectileMeshes)
	{
		if (RockMesh)
		{
			RockMesh->SetVisibility(false);
		}
	}

	bExecuteVisualActive = false;
	ExecuteVisualTimer = 0.0f;
	ActiveRockProjectiles.Reset();
}

FVector UNovaBossPatternComponent::GetGroundLocationAt(const FVector& WorldLocation) const
{
	const ACharacter* BossCharacter = GetOwnerCharacter();
	if (!BossCharacter)
	{
		return WorldLocation;
	}

	FVector GroundLocation = WorldLocation;
	GroundLocation.Z = BossCharacter->GetActorLocation().Z - BossCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + VisualGroundOffset;
	return GroundLocation;
}

void UNovaBossPatternComponent::UpdateTelegraphVisual(const float DeltaTime)
{
	if (!bShowPatternVisuals || !bPatternVisualMeshesReady)
	{
		return;
	}

	const FNovaBossAttackPattern* Pattern = GetActivePattern();
	const ACharacter* BossCharacter = GetOwnerCharacter();
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!Pattern || !BossCharacter)
	{
		return;
	}

	TelegraphVisualPulse += DeltaTime * TelegraphPulseSpeed;
	const float PulseScale = 1.0f + 0.08f * FMath::Sin(TelegraphVisualPulse);
	const FLinearColor TelegraphBaseColor = NovaBossPatternVisual::GetPatternTelegraphBaseColor(Pattern->CounterType);
	if (UMaterialInstanceDynamic* TelegraphMaterial = GetPatternTelegraphMaterial(Pattern->CounterType))
	{
		PulsePatternMaterial(TelegraphMaterial, TelegraphBaseColor, TelegraphVisualPulse);
	}

	const FVector BossLocation = BossCharacter->GetActorLocation();
	const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector GroundOrigin = GetGroundLocationAt(BossLocation);

	HidePatternVisuals();

	switch (Pattern->CounterType)
	{
	case ENovaBossCounterType::Pattern_1:
	{
		const float LaneLength = Pattern->AttackRadius;
		const FVector LaneCenter = GroundOrigin + Forward * (LaneLength * 0.5f);
		AimVisualMesh->SetMaterial(0, GetPatternTelegraphMaterial(Pattern->CounterType));
		AimVisualMesh->SetWorldLocation(LaneCenter);
		AimVisualMesh->SetWorldRotation(Forward.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(
			LaneLength / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.35f * PulseScale,
			0.05f));
		AimVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::Pattern_2:
	{
		const float HalfAngleRad = FMath::DegreesToRadians(Pattern->AttackArcDegrees * 0.5f);
		const float WedgeWidth = 2.0f * Pattern->AttackRadius * FMath::Tan(HalfAngleRad);
		const FVector WedgeCenter = GroundOrigin + Forward * (Pattern->AttackRadius * 0.5f);
		AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
		AreaVisualMesh->SetMaterial(0, GetPatternTelegraphMaterial(Pattern->CounterType));
		AreaVisualMesh->SetWorldLocation(WedgeCenter);
		AreaVisualMesh->SetWorldRotation(Forward.Rotation());
		AreaVisualMesh->SetWorldScale3D(FVector(
			Pattern->AttackRadius / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			WedgeWidth / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.05f * PulseScale));
		AreaVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::Pattern_3:
	{
		FVector TargetLocation = BossLocation + Forward * Pattern->AttackRadius;
		if (PlayerPawn)
		{
			TargetLocation = PlayerPawn->GetActorLocation();
		}

		const FVector AimDirection = (TargetLocation - BossLocation).GetSafeNormal2D();
		const float AimDistance = FMath::Min(FVector::Dist2D(BossLocation, TargetLocation), Pattern->AttackRadius);
		const FVector AimCenter = GetGroundLocationAt(BossLocation + AimDirection * (AimDistance * 0.5f));

		AimVisualMesh->SetMaterial(0, GetPatternTelegraphMaterial(Pattern->CounterType));
		AimVisualMesh->SetWorldLocation(AimCenter);
		AimVisualMesh->SetWorldRotation(AimDirection.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(
			AimDistance / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.12f * PulseScale,
			0.05f));
		AimVisualMesh->SetVisibility(true);

		if (RockProjectileMeshes.Num() > 0 && RockProjectileMeshes[0])
		{
			RockProjectileMeshes[0]->SetMaterial(0, GetPatternTelegraphMaterial(Pattern->CounterType));
			RockProjectileMeshes[0]->SetWorldLocation(GetGroundLocationAt(BossLocation + AimDirection * AimDistance) + FVector(0.0f, 0.0f, 60.0f));
			RockProjectileMeshes[0]->SetWorldScale3D(FVector(0.28f * PulseScale, 0.24f * PulseScale, 0.2f * PulseScale));
			RockProjectileMeshes[0]->SetVisibility(true);
		}
		break;
	}
	case ENovaBossCounterType::Pattern_4:
	{
		AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		AreaVisualMesh->SetMaterial(0, GetPatternTelegraphMaterial(Pattern->CounterType));
		AreaVisualMesh->SetWorldLocation(GroundOrigin);
		AreaVisualMesh->SetWorldRotation(FRotator::ZeroRotator);
		const float RadiusScale = Pattern->AttackRadius / NovaBossPatternVisual::BasicCylinderRadius;
		AreaVisualMesh->SetWorldScale3D(FVector(RadiusScale * PulseScale, RadiusScale * PulseScale, 0.06f));
		AreaVisualMesh->SetVisibility(true);
		break;
	}
	default:
		break;
	}
}

void UNovaBossPatternComponent::PlayExecuteVisual(const FNovaBossAttackPattern& Pattern)
{
	if (!bShowPatternVisuals || !bPatternVisualMeshesReady)
	{
		return;
	}

	const ACharacter* BossCharacter = GetOwnerCharacter();
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!BossCharacter)
	{
		return;
	}

	HidePatternVisuals();
	bExecuteVisualActive = true;
	ExecuteVisualTimer = ExecuteVisualSeconds;

	if (UMaterialInstanceDynamic* ExecuteMaterial = GetPatternExecuteMaterial(Pattern.CounterType))
	{
		ExecuteMaterial->SetVectorParameterValue(
			TEXT("Color"),
			NovaBossPatternVisual::GetPatternExecuteBaseColor(Pattern.CounterType));
	}

	const FVector BossLocation = BossCharacter->GetActorLocation();
	const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();

	switch (Pattern.CounterType)
	{
	case ENovaBossCounterType::Pattern_1:
	{
		const float LaneLength = Pattern.AttackRadius;
		const FVector LaneCenter = GetGroundLocationAt(BossLocation + Forward * (LaneLength * 0.5f));
		AimVisualMesh->SetMaterial(0, GetPatternExecuteMaterial(Pattern.CounterType));
		AimVisualMesh->SetWorldLocation(LaneCenter);
		AimVisualMesh->SetWorldRotation(Forward.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(LaneLength / (NovaBossPatternVisual::BasicCubeExtent * 2.0f), 0.45f, 0.07f));
		AimVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::Pattern_2:
		bExecuteVisualActive = false;
		break;
	case ENovaBossCounterType::Pattern_3:
	{
		break;
	}
	case ENovaBossCounterType::Pattern_4:
	{
		break;
	}
	default:
		bExecuteVisualActive = false;
		break;
	}
}

void UNovaBossPatternComponent::UpdateExecuteVisual(const float DeltaTime)
{
	if (!bExecuteVisualActive)
	{
		return;
	}

	ExecuteVisualTimer -= DeltaTime;
	if (ExecuteVisualTimer <= 0.0f)
	{
		HidePatternVisuals();
		if (AimVisualMesh)
		{
			AimVisualMesh->SetMaterial(0, TelegraphVisualMaterial);
		}
		if (AreaVisualMesh)
		{
			AreaVisualMesh->SetMaterial(0, TelegraphVisualMaterial);
		}
		if (ImpactVisualMesh)
		{
			ImpactVisualMesh->SetMaterial(0, TelegraphVisualMaterial);
		}
		return;
	}

	const float Alpha = 1.0f - (ExecuteVisualTimer / FMath::Max(ExecuteVisualSeconds, KINDA_SMALL_NUMBER));
	if (AreaVisualMesh && AreaVisualMesh->IsVisible())
	{
		const FVector CurrentScale = AreaVisualMesh->GetComponentScale();
		const float Pulse = 1.0f + 0.15f * (1.0f - Alpha);
		AreaVisualMesh->SetWorldScale3D(CurrentScale * FVector(Pulse, Pulse, 1.0f));
	}
	if (ImpactVisualMesh && ImpactVisualMesh->IsVisible())
	{
		ImpactVisualMesh->SetWorldScale3D(FVector(FMath::Lerp(1.4f, 0.6f, Alpha)));
	}
}
