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
	float MinConfidence = 0.55f;

	UFUNCTION(BlueprintCallable, Category = "Voice|Parser")
	FNovaVoiceCommandResult Parse(const FString& RecognizedText, float Confidence) const;

private:
	static FString NormalizeText(const FString& InText);
	static bool ContainsAnyKeyword(const FString& NormalizedText, const TArray<FString>& Keywords);
};
