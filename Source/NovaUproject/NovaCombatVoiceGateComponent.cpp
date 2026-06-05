#include "NovaCombatVoiceGateComponent.h"

#include "Engine/Engine.h"

UNovaCombatVoiceGateComponent::UNovaCombatVoiceGateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNovaCombatVoiceGateComponent::OpenCounterWindow(ENovaBossCounterType CounterType, float OverrideWindowSeconds)
{
	ActiveCounterType = CounterType;
	bCounterWindowOpen = CounterType != ENovaBossCounterType::None;
	RemainingWindowSeconds = OverrideWindowSeconds > 0.0f ? OverrideWindowSeconds : CounterWindowSeconds;

	if (GEngine && bCounterWindowOpen)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Yellow,
			FString::Printf(TEXT("Counter window open: %d"), static_cast<int32>(CounterType))
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
		return ENovaVoiceCommand::Scythe;
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
		|| Command == ENovaVoiceCommand::Scythe
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
