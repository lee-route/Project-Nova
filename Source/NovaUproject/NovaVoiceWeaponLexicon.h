#pragma once

#include "CoreMinimal.h"
#include "NovaVoiceTypes.h"

class FNovaVoiceWeaponLexicon
{
public:
	static FString ExtractHangul(const FString& In);
	static FString NormalizeRecognizedText(const FString& InText);
	static ENovaVoiceCommand MatchDirectHangul(const FString& HangulOnly);
	static ENovaVoiceCommand DetectFromRecognizedText(const FString& RecognizedText);
	static bool IsSpearHangul(const FString& HangulOnly);
	static bool IsSwordHangul(const FString& HangulOnly);
	static bool IsWeaponCommand(ENovaVoiceCommand Command);
	static float GetMinConfidenceFor(
		ENovaVoiceCommand Command,
		const FString& HangulOnly,
		float DefaultMinConfidence,
		float WeaponMinConfidence);

	static FString GetWeaponDisplayName(ENovaVoiceCommand Command);

	/** Pick STT text/confidence; trust primary direct hangul weapons (fixes 검->Bow override). */
	static bool ResolveWeaponRecognitionText(
		const TArray<TPair<FString, float>>& NBestCandidates,
		FString& OutText,
		float& OutConfidence);
};
