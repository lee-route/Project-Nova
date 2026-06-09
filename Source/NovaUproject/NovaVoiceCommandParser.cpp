#include "NovaVoiceCommandParser.h"

#include "NovaVoiceWeaponLexicon.h"

FString UNovaVoiceCommandParser::NormalizeText(const FString& InText)
{
	return FNovaVoiceWeaponLexicon::NormalizeRecognizedText(InText);
}

bool UNovaVoiceCommandParser::ContainsAnyKeyword(const FString& NormalizedText, const TArray<FString>& Keywords)
{
	for (const FString& Keyword : Keywords)
	{
		if (NormalizedText.Contains(Keyword))
		{
			return true;
		}
	}
	return false;
}

ENovaVoiceCommand UNovaVoiceCommandParser::MatchWeaponCommand(const FString& Normalized, const FString& HangulOnly) const
{
	if (const ENovaVoiceCommand DirectMatch = FNovaVoiceWeaponLexicon::MatchDirectHangul(HangulOnly);
		DirectMatch != ENovaVoiceCommand::None)
	{
		return DirectMatch;
	}

	const FString HangulFromNormalized = FNovaVoiceWeaponLexicon::ExtractHangul(Normalized);
	if (const ENovaVoiceCommand DirectFromNormalized = FNovaVoiceWeaponLexicon::MatchDirectHangul(HangulFromNormalized);
		DirectFromNormalized != ENovaVoiceCommand::None)
	{
		return DirectFromNormalized;
	}

	return FNovaVoiceWeaponLexicon::DetectFromRecognizedText(Normalized);
}

bool UNovaVoiceCommandParser::IsWeaponCommand(ENovaVoiceCommand Command)
{
	return FNovaVoiceWeaponLexicon::IsWeaponCommand(Command);
}

float UNovaVoiceCommandParser::GetRequiredConfidence(
	ENovaVoiceCommand Command,
	float DefaultMinConfidence,
	float WeaponMinConfidence)
{
	return FNovaVoiceWeaponLexicon::IsWeaponCommand(Command) ? WeaponMinConfidence : DefaultMinConfidence;
}

FNovaVoiceCommandResult UNovaVoiceCommandParser::Parse(const FString& RecognizedText, float Confidence) const
{
	FNovaVoiceCommandResult Result;
	Result.RawText = RecognizedText;
	Result.Confidence = Confidence;

	if (RecognizedText.IsEmpty())
	{
		return Result;
	}

	const FString Normalized = NormalizeText(RecognizedText);
	FString HangulOnly = FNovaVoiceWeaponLexicon::ExtractHangul(Normalized);

	if (ContainsAnyKeyword(Normalized, {TEXT("취소"), TEXT("cancel"), TEXT("stop"), TEXT("그만")}))
	{
		Result.Command = ENovaVoiceCommand::Cancel;
	}
	else if (ContainsAnyKeyword(
		Normalized,
		{TEXT("도와"), TEXT("도움"), TEXT("help"), TEXT("헬프"), TEXT("헬"), TEXT("도와줘"), TEXT("도와죠"), TEXT("도와주")}))
	{
		Result.Command = ENovaVoiceCommand::Help;
	}
	else
	{
		Result.Command = MatchWeaponCommand(Normalized, HangulOnly);
		if (Result.Command == ENovaVoiceCommand::None)
		{
			Result.Command = FNovaVoiceWeaponLexicon::DetectFromRecognizedText(RecognizedText);
		}
	}

	if (Result.Command != ENovaVoiceCommand::None)
	{
		HangulOnly = FNovaVoiceWeaponLexicon::ExtractHangul(Normalized);
		if (HangulOnly.IsEmpty())
		{
			HangulOnly = FNovaVoiceWeaponLexicon::ExtractHangul(RecognizedText);
		}
	}

	if (IsWeaponCommand(Result.Command))
	{
		Result.bAccepted = true;
		return Result;
	}

	const float RequiredConfidence = FNovaVoiceWeaponLexicon::GetMinConfidenceFor(
		Result.Command,
		HangulOnly,
		MinConfidence,
		MinWeaponConfidence);

	Result.bAccepted = Result.Command != ENovaVoiceCommand::None && Confidence >= RequiredConfidence;
	return Result;
}
