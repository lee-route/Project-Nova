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

	/** 짧은 무기 단어(창/활 등)는 Azure confidence가 낮게 나오는 경우가 많음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Parser")
	float MinWeaponConfidence = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Voice|Parser")
	FNovaVoiceCommandResult Parse(const FString& RecognizedText, float Confidence) const;

private:
	static FString NormalizeText(const FString& InText);
	static bool ContainsAnyKeyword(const FString& NormalizedText, const TArray<FString>& Keywords);
	static bool IsWeaponCommand(ENovaVoiceCommand Command);
	static float GetRequiredConfidence(ENovaVoiceCommand Command, float DefaultMinConfidence, float WeaponMinConfidence);
	ENovaVoiceCommand MatchWeaponCommand(const FString& Normalized, const FString& HangulOnly) const;
};
