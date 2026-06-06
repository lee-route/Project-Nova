#include "NovaVoiceCommandParser.h"

FString UNovaVoiceCommandParser::NormalizeText(const FString& InText)
{
	FString Normalized = InText;
	Normalized.TrimStartAndEndInline();
	Normalized = Normalized.ToLower();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("."), TEXT(""));
	Normalized.ReplaceInline(TEXT(","), TEXT(""));
	Normalized.ReplaceInline(TEXT("!"), TEXT(""));
	Normalized.ReplaceInline(TEXT("?"), TEXT(""));
	return Normalized;
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

	if (ContainsAnyKeyword(Normalized, {TEXT("취소"), TEXT("cancel"), TEXT("stop")}))
	{
		Result.Command = ENovaVoiceCommand::Cancel;
	}
	else if (ContainsAnyKeyword(Normalized, {TEXT("도와"), TEXT("help")}))
	{
		Result.Command = ENovaVoiceCommand::Help;
	}
	else if (ContainsAnyKeyword(Normalized, {TEXT("방패"), TEXT("shield")}))
	{
		Result.Command = ENovaVoiceCommand::Shield;
	}
	else if (ContainsAnyKeyword(Normalized, {TEXT("낫"), TEXT("scythe")}))
	{
		Result.Command = ENovaVoiceCommand::Scythe;
	}
	else if (ContainsAnyKeyword(Normalized, {TEXT("망치"), TEXT("hammer")}))
	{
		Result.Command = ENovaVoiceCommand::Hammer;
	}
	else if (ContainsAnyKeyword(Normalized, {TEXT("활"), TEXT("bow"), TEXT("궁")}))
	{
		Result.Command = ENovaVoiceCommand::Bow;
	}

	Result.bAccepted = Result.Command != ENovaVoiceCommand::None && Confidence >= MinConfidence;
	return Result;
}
