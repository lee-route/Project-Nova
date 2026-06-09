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
	/** 돌진(Pattern_1) → 방패 */
	Pattern_1 UMETA(DisplayName = "Pattern 1 - Dash"),
	/** 범위공격(Pattern_2) → 창 */
	Pattern_2 UMETA(DisplayName = "Pattern 2 - Area Attack"),
	/** 투사체(Pattern_3) → 활 */
	Pattern_3 UMETA(DisplayName = "Pattern 3 - Projectile"),
	/** 패턴_@ 미정(Pattern_4) → 검 */
	Pattern_4 UMETA(DisplayName = "Pattern 4 - TBD")
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
