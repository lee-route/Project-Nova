#include "NovaCombatVoiceGateComponent.h"

#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "NovaParagonGruxBossComponent.h"

UNovaCombatVoiceGateComponent::UNovaCombatVoiceGateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNovaCombatVoiceGateComponent::BeginPlay()
{
	Super::BeginPlay();

	const FString LocalConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("LocalNovaVoice.ini"));
	if (GConfig->GetFloat(TEXT("/Script/NovaUproject.NovaVoiceSettings"), TEXT("CounterWindowSeconds"), CounterWindowSeconds, GGameIni))
	{
		// DefaultNovaVoice.ini
	}

	if (FPaths::FileExists(LocalConfigPath))
	{
		GConfig->GetFloat(TEXT("/Script/NovaUproject.NovaVoiceSettings"), TEXT("CounterWindowSeconds"), CounterWindowSeconds, LocalConfigPath);
		GConfig->GetFloat(TEXT("/Script/NovaUproject.NovaVoiceSettings"), TEXT("MaxCounterBossDistance"), MaxCounterBossDistance, LocalConfigPath);
	}

	GConfig->GetFloat(TEXT("/Script/NovaUproject.NovaVoiceSettings"), TEXT("MaxCounterBossDistance"), MaxCounterBossDistance, GGameIni);
}

bool UNovaCombatVoiceGateComponent::ValidateCounterBossSource(AActor* BossSource, FString& OutRejectReason) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	const ACharacter* PlayerCharacter = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr;
	return UNovaParagonGruxBossComponent::CanActorServeAsCounterBoss(
		BossSource,
		PlayerCharacter,
		MaxCounterBossDistance,
		OutRejectReason);
}

bool UNovaCombatVoiceGateComponent::IsActiveCounterBossStillValid(FString& OutRejectReason) const
{
	return ValidateCounterBossSource(ActiveCounterBossSource.Get(), OutRejectReason);
}

void UNovaCombatVoiceGateComponent::InvalidateCounterBossIfNeeded()
{
	if (!bCounterWindowOpen)
	{
		return;
	}

	FString RejectReason;
	if (!IsActiveCounterBossStillValid(RejectReason))
	{
		if (GEngine && !RejectReason.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, RejectReason);
		}
		CloseCounterWindow();
	}
}

bool UNovaCombatVoiceGateComponent::OpenCounterWindow(
	ENovaBossCounterType CounterType,
	AActor* BossSource,
	float OverrideWindowSeconds)
{
	if (CounterType == ENovaBossCounterType::None)
	{
		CloseCounterWindow();
		return false;
	}

	FString RejectReason;
	if (!ValidateCounterBossSource(BossSource, RejectReason))
	{
		if (GEngine && !RejectReason.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, RejectReason);
		}
		return false;
	}

	ActiveCounterBossSource = BossSource;
	ActiveCounterType = CounterType;
	bCounterWindowOpen = true;
	RemainingWindowSeconds = OverrideWindowSeconds > 0.0f ? OverrideWindowSeconds : CounterWindowSeconds;

	if (GEngine && bCounterWindowOpen)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			CounterWindowSeconds,
			FColor::Yellow,
			FString::Printf(
				TEXT("상쇄 창: %s - %s (%.1f초)"),
				*GetPatternFullLabel(CounterType),
				*GetRequiredWeaponCounterLabel(CounterType),
				RemainingWindowSeconds));
	}

	if (bCounterWindowOpen)
	{
		OnCounterWindowOpened.Broadcast(CounterType);
	}

	return true;
}

void UNovaCombatVoiceGateComponent::CloseCounterWindow()
{
	bCounterWindowOpen = false;
	ActiveCounterType = ENovaBossCounterType::None;
	ActiveCounterBossSource.Reset();
	RemainingWindowSeconds = 0.0f;
}

void UNovaCombatVoiceGateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCounterWindowOpen)
	{
		return;
	}

	InvalidateCounterBossIfNeeded();
	if (!bCounterWindowOpen)
	{
		return;
	}

	RemainingWindowSeconds -= DeltaTime;
	if (RemainingWindowSeconds <= 0.0f)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Red,
				FString::Printf(
					TEXT("상쇄 실패: %s 시간 초과"),
					*GetPatternFullLabel(ActiveCounterType)));
		}
		CloseCounterWindow();
	}
}

ENovaVoiceCommand UNovaCombatVoiceGateComponent::GetRequiredCommandForCounter(ENovaBossCounterType CounterType)
{
	return GetRequiredWeaponForPattern(CounterType);
}

ENovaVoiceCommand UNovaCombatVoiceGateComponent::GetRequiredWeaponForPattern(ENovaBossCounterType CounterType)
{
	switch (CounterType)
	{
	case ENovaBossCounterType::Pattern_1:
		return ENovaVoiceCommand::Shield;
	case ENovaBossCounterType::Pattern_2:
		return ENovaVoiceCommand::Spear;
	case ENovaBossCounterType::Pattern_3:
		return ENovaVoiceCommand::Bow;
	case ENovaBossCounterType::Pattern_4:
		return ENovaVoiceCommand::Hammer;
	default:
		return ENovaVoiceCommand::None;
	}
}

FString UNovaCombatVoiceGateComponent::GetPatternCodeName(ENovaBossCounterType CounterType)
{
	switch (CounterType)
	{
	case ENovaBossCounterType::Pattern_1: return TEXT("Pattern_1");
	case ENovaBossCounterType::Pattern_2: return TEXT("Pattern_2");
	case ENovaBossCounterType::Pattern_3: return TEXT("Pattern_3");
	case ENovaBossCounterType::Pattern_4: return TEXT("Pattern_4");
	default: return TEXT("None");
	}
}

FString UNovaCombatVoiceGateComponent::GetPatternDisplayName(ENovaBossCounterType CounterType)
{
	switch (CounterType)
	{
	case ENovaBossCounterType::Pattern_1:
		return TEXT("돌진");
	case ENovaBossCounterType::Pattern_2:
		return TEXT("범위공격");
	case ENovaBossCounterType::Pattern_3:
		return TEXT("투사체");
	case ENovaBossCounterType::Pattern_4:
		return TEXT("패턴_@");
	default:
		return TEXT("없음");
	}
}

FString UNovaCombatVoiceGateComponent::GetPatternFullLabel(ENovaBossCounterType CounterType)
{
	if (CounterType == ENovaBossCounterType::None)
	{
		return GetPatternDisplayName(CounterType);
	}

	if (CounterType == ENovaBossCounterType::Pattern_4)
	{
		return FString::Printf(TEXT("%s(미정, %s)"), *GetPatternDisplayName(CounterType), *GetPatternCodeName(CounterType));
	}

	return FString::Printf(TEXT("%s(%s)"), *GetPatternDisplayName(CounterType), *GetPatternCodeName(CounterType));
}

FString UNovaCombatVoiceGateComponent::GetRequiredWeaponDisplayName(ENovaBossCounterType CounterType)
{
	return GetWeaponDisplayNameFromCommand(GetRequiredWeaponForPattern(CounterType));
}

FString UNovaCombatVoiceGateComponent::GetRequiredWeaponCounterLabel(ENovaBossCounterType CounterType)
{
	return FString::Printf(TEXT("%s로 상쇄"), *GetRequiredWeaponDisplayName(CounterType));
}

FString UNovaCombatVoiceGateComponent::GetWeaponDisplayNameFromCommand(ENovaVoiceCommand Command)
{
	switch (Command)
	{
	case ENovaVoiceCommand::Shield:
		return TEXT("방패");
	case ENovaVoiceCommand::Spear:
		return TEXT("창");
	case ENovaVoiceCommand::Bow:
		return TEXT("활");
	case ENovaVoiceCommand::Hammer:
		return TEXT("검");
	default:
		return TEXT("?");
	}
}

bool UNovaCombatVoiceGateComponent::IsWeaponSwitchCommand(ENovaVoiceCommand Command)
{
	return Command == ENovaVoiceCommand::Bow
		|| Command == ENovaVoiceCommand::Shield
		|| Command == ENovaVoiceCommand::Spear
		|| Command == ENovaVoiceCommand::Hammer;
}

bool UNovaCombatVoiceGateComponent::AcceptCounterSuccess(ENovaVoiceCommand Command)
{
	if (!bCounterWindowOpen)
	{
		return false;
	}

	FString RejectReason;
	if (!IsActiveCounterBossStillValid(RejectReason))
	{
		if (GEngine && !RejectReason.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, RejectReason);
		}
		CloseCounterWindow();
		return false;
	}

	if (AActor* BossSource = ActiveCounterBossSource.Get())
	{
		if (UNovaParagonGruxBossComponent* GruxComponent = BossSource->FindComponentByClass<UNovaParagonGruxBossComponent>())
		{
			GruxComponent->NotifyGruxCounterSucceeded(ActiveCounterType, Command);
		}
	}

	OnCounterSucceeded.Broadcast(ActiveCounterType, Command);
	CloseCounterWindow();
	return true;
}

bool UNovaCombatVoiceGateComponent::TryCounterWithEquippedWeapon(ENovaVoiceCommand EquippedWeapon, FString& OutRejectReason)
{
	if (!bCounterWindowOpen)
	{
		return true;
	}

	if (!IsWeaponSwitchCommand(EquippedWeapon))
	{
		OutRejectReason = TEXT("상쇄 창: 무기 명령이 필요합니다");
		return false;
	}

	const ENovaVoiceCommand Required = GetRequiredCommandForCounter(ActiveCounterType);
	if (EquippedWeapon != Required)
	{
		OutRejectReason = FString::Printf(
			TEXT("상쇄 실패: %s → %s (현재 %s)"),
			*GetPatternFullLabel(ActiveCounterType),
			*GetRequiredWeaponCounterLabel(ActiveCounterType),
			*GetWeaponDisplayNameFromCommand(EquippedWeapon));
		return false;
	}

	return AcceptCounterSuccess(EquippedWeapon);
}

bool UNovaCombatVoiceGateComponent::TryAcceptVoiceCommand(const FNovaVoiceCommandResult& CommandResult, FString& OutRejectReason)
{
	if (!CommandResult.bAccepted)
	{
		OutRejectReason = TEXT("Parser rejected command");
		return false;
	}

	if (bCounterWindowOpen)
	{
		const ENovaVoiceCommand Required = GetRequiredCommandForCounter(ActiveCounterType);
		if (CommandResult.Command != Required)
		{
			OutRejectReason = FString::Printf(
				TEXT("상쇄 실패: %s → %s"),
				*GetPatternFullLabel(ActiveCounterType),
				*GetRequiredWeaponCounterLabel(ActiveCounterType));
			return false;
		}

		return AcceptCounterSuccess(CommandResult.Command);
	}

	if (IsWeaponSwitchCommand(CommandResult.Command) || CommandResult.Command == ENovaVoiceCommand::Help)
	{
		return true;
	}

	OutRejectReason = TEXT("Command not allowed outside counter window");
	return false;
}

bool UNovaCombatVoiceGateComponent::TryResolveCounterWindow(const FNovaVoiceCommandResult& CommandResult, FString& OutRejectReason)
{
	if (!bCounterWindowOpen)
	{
		return true;
	}

	if (!IsWeaponSwitchCommand(CommandResult.Command))
	{
		OutRejectReason = TEXT("상쇄 창: 무기 음성 명령이 필요합니다");
		return false;
	}

	const ENovaVoiceCommand Required = GetRequiredCommandForCounter(ActiveCounterType);
	if (CommandResult.Command != Required)
	{
		OutRejectReason = FString::Printf(
			TEXT("상쇄 실패: %s → %s"),
			*GetPatternFullLabel(ActiveCounterType),
			*GetRequiredWeaponCounterLabel(ActiveCounterType));
		return false;
	}

	if (!CommandResult.bAccepted)
	{
		OutRejectReason = TEXT("음성 인식 신뢰도 부족");
		return false;
	}

	return AcceptCounterSuccess(CommandResult.Command);
}
