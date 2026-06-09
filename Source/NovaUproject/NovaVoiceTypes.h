#pragma once

#include "CoreMinimal.h"
#include "NovaVoiceTypes.generated.h"

UENUM(BlueprintType)
enum class ENovaVoiceCommand : uint8
{
	None UMETA(DisplayName = "None"),
	Bow UMETA(DisplayName = "Bow"),
	Shield UMETA(DisplayName = "Shield"),
	Spear UMETA(DisplayName = "Spear"),
	Hammer UMETA(DisplayName = "Sword"),
	Help UMETA(DisplayName = "Help"),
	Cancel UMETA(DisplayName = "Cancel")
};

UENUM(BlueprintType)
enum class ENovaBossCounterType : uint8
{
	None UMETA(DisplayName = "None"),
	LaserShield UMETA(DisplayName = "Charge Rush"),
	SpaceScythe UMETA(DisplayName = "AoE 45"),
	SummonBow UMETA(DisplayName = "Projectile"),
	DebrisHammer UMETA(DisplayName = "AoE 360")
};

USTRUCT(BlueprintType)
struct FNovaVoiceCommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	ENovaVoiceCommand Command = ENovaVoiceCommand::None;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	FString RawText;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	float Confidence = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	bool bAccepted = false;
};

USTRUCT(BlueprintType)
struct FNovaVoiceLatencyStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	int32 TotalRequests = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	int32 SuccessfulRequests = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	int32 FailedRequests = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	int32 RejectedCommands = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	float LastRoundTripMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	float AverageRoundTripMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Voice")
	float MaxRoundTripMs = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNovaVoiceCommandDelegate, const FNovaVoiceCommandResult&, Result);
