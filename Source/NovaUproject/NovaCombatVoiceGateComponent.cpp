#include "NovaCombatVoiceGateComponent.h"

#include "NovaVoiceWeaponLexicon.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace NovaBossCounterNames
{
	static FString GetPatternName(const ENovaBossCounterType CounterType)
	{
		switch (CounterType)
		{
		case ENovaBossCounterType::LaserShield: return TEXT("돌진");
		case ENovaBossCounterType::SpaceScythe: return TEXT("범위공격(45°)");
		case ENovaBossCounterType::SummonBow: return TEXT("투사체");
		case ENovaBossCounterType::DebrisHammer: return TEXT("범위공격(360°)");
		default: return TEXT("?");
		}
	}
}

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
	}
}

void UNovaCombatVoiceGateComponent::OpenCounterWindow(ENovaBossCounterType CounterType, float OverrideWindowSeconds)
{
	ActiveCounterType = CounterType;
	bCounterWindowOpen = CounterType != ENovaBossCounterType::None;
	RemainingWindowSeconds = OverrideWindowSeconds > 0.0f ? OverrideWindowSeconds : CounterWindowSeconds;

	if (GEngine && bCounterWindowOpen)
	{
		const ENovaVoiceCommand RequiredWeapon = GetRequiredCommandForCounter(CounterType);
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Yellow,
			FString::Printf(
				TEXT("상쇄: %s → %s (키 1~4 또는 음성)"),
				*NovaBossCounterNames::GetPatternName(CounterType),
				*FNovaVoiceWeaponLexicon::GetWeaponDisplayName(RequiredWeapon)
			)
		);
	}
}

void UNovaCombatVoiceGateComponent::CloseCounterWindow()
{
	bCounterWindowOpen = false;
	ActiveCounterType = ENovaBossCounterType::None;
	RemainingWindowSeconds = 0.0f;
}

void UNovaCombatVoiceGateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCounterWindowOpen)
	{
		return;
	}

	RemainingWindowSeconds -= DeltaTime;
	if (RemainingWindowSeconds <= 0.0f)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, TEXT("Counter window missed"));
		}
		CloseCounterWindow();
	}
}

ENovaVoiceCommand UNovaCombatVoiceGateComponent::GetRequiredCommandForCounter(ENovaBossCounterType CounterType)
{
	switch (CounterType)
	{
	case ENovaBossCounterType::LaserShield:
		return ENovaVoiceCommand::Shield;
	case ENovaBossCounterType::SpaceScythe:
		return ENovaVoiceCommand::Spear;
	case ENovaBossCounterType::SummonBow:
		return ENovaVoiceCommand::Bow;
	case ENovaBossCounterType::DebrisHammer:
		return ENovaVoiceCommand::Hammer;
	default:
		return ENovaVoiceCommand::None;
	}
}

bool UNovaCombatVoiceGateComponent::IsWeaponSwitchCommand(ENovaVoiceCommand Command)
{
	return Command == ENovaVoiceCommand::Bow
		|| Command == ENovaVoiceCommand::Shield
		|| Command == ENovaVoiceCommand::Spear
		|| Command == ENovaVoiceCommand::Hammer;
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
				TEXT("Counter window requires command %d, got %d"),
				static_cast<int32>(Required),
				static_cast<int32>(CommandResult.Command)
			);
			return false;
		}

		OnCounterSucceeded.Broadcast(ActiveCounterType, CommandResult.Command);
		CloseCounterWindow();
		return true;
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
		OutRejectReason = TEXT("Counter window requires a weapon voice command");
		return false;
	}

	const ENovaVoiceCommand Required = GetRequiredCommandForCounter(ActiveCounterType);
	if (CommandResult.Command != Required)
	{
		OutRejectReason = FString::Printf(
			TEXT("Counter window requires command %d, got %d"),
			static_cast<int32>(Required),
			static_cast<int32>(CommandResult.Command)
		);
		return false;
	}

	OnCounterSucceeded.Broadcast(ActiveCounterType, CommandResult.Command);
	CloseCounterWindow();
	return true;
}
