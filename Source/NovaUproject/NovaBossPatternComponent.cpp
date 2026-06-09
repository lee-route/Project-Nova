#include "NovaBossPatternComponent.h"

#include "NovaClickMovePlayerController.h"
#include "NovaCombatVoiceGateComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
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
			TEXT("Boss_Charge"),
			ENovaBossCounterType::LaserShield,
			1.5f,
			1.4f,
			28.0f,
			500.0f,
			0.0f,
			true,
			nullptr
		},
		{
			TEXT("Boss_AoE45"),
			ENovaBossCounterType::SpaceScythe,
			1.6f,
			1.3f,
			32.0f,
			420.0f,
			45.0f,
			true,
			nullptr
		},
		{
			TEXT("Boss_Projectile"),
			ENovaBossCounterType::SummonBow,
			1.4f,
			1.5f,
			22.0f,
			650.0f,
			0.0f,
			true,
			nullptr
		},
		{
			TEXT("Boss_AoE360"),
			ENovaBossCounterType::DebrisHammer,
			1.7f,
			1.6f,
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
	BindPlayerCounterDelegate();

	if (bAutoStartOnBeginPlay)
	{
		StartBossAI();
	}
}

void UNovaBossPatternComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HidePatternVisuals();
	UnbindPlayerCounterDelegate();
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

void UNovaBossPatternComponent::BindPlayerCounterDelegate()
{
	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}

	UNovaCombatVoiceGateComponent* VoiceGate = PlayerController->FindComponentByClass<UNovaCombatVoiceGateComponent>();
	if (!VoiceGate)
	{
		return;
	}

	if (BoundVoiceGate.Get() == VoiceGate)
	{
		return;
	}

	UnbindPlayerCounterDelegate();
	VoiceGate->OnCounterSucceeded.AddDynamic(this, &UNovaBossPatternComponent::HandleCounterSucceeded);
	BoundVoiceGate = VoiceGate;
}

void UNovaBossPatternComponent::UnbindPlayerCounterDelegate()
{
	if (UNovaCombatVoiceGateComponent* VoiceGate = BoundVoiceGate.Get())
	{
		VoiceGate->OnCounterSucceeded.RemoveDynamic(this, &UNovaBossPatternComponent::HandleCounterSucceeded);
	}

	BoundVoiceGate.Reset();
}

void UNovaBossPatternComponent::HandleCounterSucceeded(ENovaBossCounterType CounterType, ENovaVoiceCommand Command)
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
	bCounteredThisPattern = true;
	EnterStaggered();
	OnPatternCountered.Broadcast(CounterType);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Emerald,
			FString::Printf(TEXT("Grux pattern countered: %d"), static_cast<int32>(CounterType))
		);
	}
}

void UNovaBossPatternComponent::FacePlayer()
{
	ACharacter* BossCharacter = GetOwnerCharacter();
	const APawn* PlayerPawn = GetPlayerPawn();
	if (!BossCharacter || !PlayerPawn)
	{
		return;
	}

	FVector ToPlayer = PlayerPawn->GetActorLocation() - BossCharacter->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (!ToPlayer.IsNearlyZero())
	{
		BossCharacter->SetActorRotation(ToPlayer.Rotation());
	}
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
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (ANovaClickMovePlayerController* NovaController = Cast<ANovaClickMovePlayerController>(PlayerController))
			{
				NovaController->OpenBossCounterWindow(Pattern.CounterType);
			}
		}
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
	const FNovaBossAttackPattern* Pattern = GetActivePattern();
	if (!Pattern || bCounteredThisPattern)
	{
		return;
	}

	CurrentState = ENovaBossPatternState::Executing;
	FacePlayer();

	APawn* PlayerPawn = GetPlayerPawn();
	ACharacter* BossCharacter = GetOwnerCharacter();
	if (PlayerPawn && BossCharacter)
	{
		const FVector BossLocation = BossCharacter->GetActorLocation();
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		const float Distance = FVector::Dist2D(BossLocation, PlayerLocation);
		bool bInArc = true;

		if (Pattern->AttackArcDegrees > 0.0f && Pattern->AttackArcDegrees < 360.0f)
		{
			const FVector ToPlayer = (PlayerLocation - BossLocation).GetSafeNormal2D();
			const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
			const float Dot = FVector::DotProduct(Forward, ToPlayer);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
			bInArc = AngleDeg <= (Pattern->AttackArcDegrees * 0.5f);
		}

		if (Pattern->CounterType == ENovaBossCounterType::LaserShield)
		{
			const FVector DashDirection = (PlayerLocation - BossLocation).GetSafeNormal2D();
			if (!DashDirection.IsNearlyZero())
			{
				BossCharacter->LaunchCharacter(DashDirection * ChargeDashStrength + FVector(0.0f, 0.0f, 120.0f), true, true);
			}
		}

		if (Distance <= Pattern->AttackRadius && bInArc)
		{
			UGameplayStatics::ApplyDamage(
				PlayerPawn,
				Pattern->Damage,
				BossCharacter->GetController(),
				BossCharacter,
				nullptr);
		}
	}

	PlayExecuteVisual(*Pattern);
	OnPatternExecuted.Broadcast(Pattern->CounterType, ActivePatternIndex);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Red,
			FString::Printf(TEXT("Grux hit: %s (%.0f dmg)"), *Pattern->PatternId.ToString(), Pattern->Damage)
		);
	}

	CurrentState = ENovaBossPatternState::Recovering;
	StateTimer = Pattern->RecoverySeconds;
	AdvancePatternIndex();
}

void UNovaBossPatternComponent::EnterStaggered()
{
	CurrentState = ENovaBossPatternState::Staggered;
	StateTimer = StaggerDuration;
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
	FacePlayer();
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

void UNovaBossPatternComponent::UpdateExecuting()
{
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

	BindPlayerCounterDelegate();

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
		UpdateExecuting();
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

	AreaVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternAreaVisual"), CylinderMesh, TelegraphVisualMaterial);
	AimVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternAimVisual"), CubeMesh, TelegraphVisualMaterial);
	ImpactVisualMesh = NovaBossPatternVisual::CreateVisualMesh(Owner, TEXT("BossPatternImpactVisual"), SphereMesh, ExecuteVisualMaterial);

	bPatternVisualMeshesReady = AreaVisualMesh != nullptr && AimVisualMesh != nullptr && ImpactVisualMesh != nullptr;
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

	bExecuteVisualActive = false;
	ExecuteVisualTimer = 0.0f;
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
	if (TelegraphVisualMaterial)
	{
		const float PulseColor = 0.75f + 0.25f * FMath::Sin(TelegraphVisualPulse);
		TelegraphVisualMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.85f, 0.1f, 1.0f) * PulseColor);
	}

	const FVector BossLocation = BossCharacter->GetActorLocation();
	const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
	const FVector GroundOrigin = GetGroundLocationAt(BossLocation);

	HidePatternVisuals();

	switch (Pattern->CounterType)
	{
	case ENovaBossCounterType::LaserShield:
	{
		const float LaneLength = Pattern->AttackRadius;
		const FVector LaneCenter = GroundOrigin + Forward * (LaneLength * 0.5f);
		AimVisualMesh->SetWorldLocation(LaneCenter);
		AimVisualMesh->SetWorldRotation(Forward.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(
			LaneLength / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.35f * PulseScale,
			0.05f));
		AimVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::SpaceScythe:
	{
		const float HalfAngleRad = FMath::DegreesToRadians(Pattern->AttackArcDegrees * 0.5f);
		const float WedgeWidth = 2.0f * Pattern->AttackRadius * FMath::Tan(HalfAngleRad);
		const FVector WedgeCenter = GroundOrigin + Forward * (Pattern->AttackRadius * 0.5f);
		AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
		AreaVisualMesh->SetWorldLocation(WedgeCenter);
		AreaVisualMesh->SetWorldRotation(Forward.Rotation());
		AreaVisualMesh->SetWorldScale3D(FVector(
			Pattern->AttackRadius / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			WedgeWidth / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.05f * PulseScale));
		AreaVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::SummonBow:
	{
		FVector TargetLocation = BossLocation + Forward * Pattern->AttackRadius;
		if (PlayerPawn)
		{
			TargetLocation = PlayerPawn->GetActorLocation();
		}

		const FVector AimDirection = (TargetLocation - BossLocation).GetSafeNormal2D();
		const float AimDistance = FMath::Min(FVector::Dist2D(BossLocation, TargetLocation), Pattern->AttackRadius);
		const FVector AimCenter = GetGroundLocationAt(BossLocation + AimDirection * (AimDistance * 0.5f));

		AimVisualMesh->SetWorldLocation(AimCenter);
		AimVisualMesh->SetWorldRotation(AimDirection.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(
			AimDistance / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.12f * PulseScale,
			0.05f));
		AimVisualMesh->SetVisibility(true);

		ImpactVisualMesh->SetWorldLocation(GetGroundLocationAt(BossLocation + AimDirection * AimDistance) + FVector(0.0f, 0.0f, 60.0f));
		ImpactVisualMesh->SetWorldScale3D(FVector(0.35f * PulseScale));
		ImpactVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::DebrisHammer:
	{
		AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
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

	if (ExecuteVisualMaterial)
	{
		ExecuteVisualMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.1f, 0.05f, 1.0f));
	}

	const FVector BossLocation = BossCharacter->GetActorLocation();
	const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();

	switch (Pattern.CounterType)
	{
	case ENovaBossCounterType::LaserShield:
	{
		const float LaneLength = Pattern.AttackRadius;
		const FVector LaneCenter = GetGroundLocationAt(BossLocation + Forward * (LaneLength * 0.5f));
		AimVisualMesh->SetMaterial(0, ExecuteVisualMaterial);
		AimVisualMesh->SetWorldLocation(LaneCenter);
		AimVisualMesh->SetWorldRotation(Forward.Rotation());
		AimVisualMesh->SetWorldScale3D(FVector(LaneLength / (NovaBossPatternVisual::BasicCubeExtent * 2.0f), 0.45f, 0.07f));
		AimVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::SpaceScythe:
	{
		const float HalfAngleRad = FMath::DegreesToRadians(Pattern.AttackArcDegrees * 0.5f);
		const float WedgeWidth = 2.0f * Pattern.AttackRadius * FMath::Tan(HalfAngleRad);
		const FVector WedgeCenter = GetGroundLocationAt(BossLocation + Forward * (Pattern.AttackRadius * 0.5f));
		AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube")));
		AreaVisualMesh->SetMaterial(0, ExecuteVisualMaterial);
		AreaVisualMesh->SetWorldLocation(WedgeCenter);
		AreaVisualMesh->SetWorldRotation(Forward.Rotation());
		AreaVisualMesh->SetWorldScale3D(FVector(
			Pattern.AttackRadius / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			WedgeWidth / (NovaBossPatternVisual::BasicCubeExtent * 2.0f),
			0.08f));
		AreaVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::SummonBow:
	{
		FVector TargetLocation = BossLocation + Forward * Pattern.AttackRadius;
		if (PlayerPawn)
		{
			TargetLocation = PlayerPawn->GetActorLocation();
		}

		ProjectileVisualStart = BossLocation + FVector(0.0f, 0.0f, 120.0f);
		ProjectileVisualEnd = TargetLocation + FVector(0.0f, 0.0f, 80.0f);
		ImpactVisualMesh->SetMaterial(0, ExecuteVisualMaterial);
		ImpactVisualMesh->SetWorldLocation(ProjectileVisualStart);
		ImpactVisualMesh->SetWorldScale3D(FVector(0.25f));
		ImpactVisualMesh->SetVisibility(true);
		break;
	}
	case ENovaBossCounterType::DebrisHammer:
	{
		AreaVisualMesh->SetStaticMesh(NovaBossPatternVisual::LoadMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		AreaVisualMesh->SetMaterial(0, ExecuteVisualMaterial);
		AreaVisualMesh->SetWorldLocation(GetGroundLocationAt(BossLocation));
		AreaVisualMesh->SetWorldRotation(FRotator::ZeroRotator);
		const float RadiusScale = Pattern.AttackRadius / NovaBossPatternVisual::BasicCylinderRadius;
		AreaVisualMesh->SetWorldScale3D(FVector(RadiusScale, RadiusScale, 0.1f));
		AreaVisualMesh->SetVisibility(true);
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

	const FNovaBossAttackPattern* Pattern = GetActivePattern();
	if (!Pattern || Pattern->CounterType != ENovaBossCounterType::SummonBow || !ImpactVisualMesh)
	{
		return;
	}

	const float Alpha = 1.0f - (ExecuteVisualTimer / FMath::Max(ExecuteVisualSeconds, KINDA_SMALL_NUMBER));
	const FVector ProjectileLocation = FMath::Lerp(ProjectileVisualStart, ProjectileVisualEnd, Alpha);
	ImpactVisualMesh->SetWorldLocation(ProjectileLocation);
	ImpactVisualMesh->SetWorldScale3D(FVector(FMath::Lerp(0.25f, 0.7f, Alpha)));
}
