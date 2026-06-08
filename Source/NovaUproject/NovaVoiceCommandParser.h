#pragma once

#include "CoreMinimal.h"
#include "NovaVoiceTypes.h"
#include "NovaVoiceCommandParser.generated.h"

UCLASS(BlueprintType)
class NOVAUPROJECT_API UNovaVoiceCommandParser : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Parser")
	float MinConfidence = 0.40f;

	/** Short weapon words (낫/활 등) often come back with low Azure confidence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Parser")
	float MinWeaponConfidence = 0.05f;

	UFUNCTION(BlueprintCallable, Category = "Voice|Parser")
	FNovaVoiceCommandResult Parse(const FString& RecognizedText, float Confidence) const;

private:
	static FString NormalizeText(const FString& InText);
	static bool ContainsAnyKeyword(const FString& NormalizedText, const TArray<FString>& Keywords);
	static bool IsWeaponCommand(ENovaVoiceCommand Command);
	static float GetRequiredConfidence(ENovaVoiceCommand Command, float DefaultMinConfidence, float WeaponMinConfidence);
	ENovaVoiceCommand MatchWeaponCommand(const FString& Normalized, const FString& HangulOnly) const;
};
