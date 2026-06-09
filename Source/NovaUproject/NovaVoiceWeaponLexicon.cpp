#include "NovaVoiceWeaponLexicon.h"

namespace
{
	static const TCHAR* G_ShieldContains[] = {
		TEXT("방패"), TEXT("방"), TEXT("빵패"), TEXT("반패"), TEXT("방페"), TEXT("뱅패"), TEXT("방때"),
		TEXT("shield"), TEXT("실드"), TEXT("쉴드"), TEXT("쉴"), nullptr
	};
	static const TCHAR* G_ShieldExact[] = {
		TEXT("방"), TEXT("뱅"), TEXT("반"), TEXT("빵"), TEXT("방때"), TEXT("빵패"), TEXT("반패"), TEXT("방페"), TEXT("방패"),
		TEXT("방이"), TEXT("빵"), TEXT("밥"), nullptr
	};

	// 창이 기본. 낫/나/낮 등은 Azure STT가 창을 잘못 쓸 때 흔한 오인식 별칭.
	static const TCHAR* G_SpearContains[] = {
		TEXT("창"), TEXT("챙"), TEXT("장"), TEXT("상"), TEXT("spear"), TEXT("lance"), TEXT("chang"),
		TEXT("낫"), TEXT("scythe"), TEXT("사이드"), TEXT("싸이"), TEXT("낮"), TEXT("산"), nullptr
	};
	static const TCHAR* G_SpearExact[] = {
		TEXT("창"), TEXT("챙"), TEXT("장"), TEXT("상"), TEXT("창이"), TEXT("창을"),
		TEXT("나"), TEXT("낮"), TEXT("낳"), TEXT("닷"), TEXT("낫"), TEXT("nat"), TEXT("nit"), TEXT("not"),
		TEXT("넛"), TEXT("랫"), TEXT("낧"), TEXT("낟"), TEXT("났"), TEXT("냇"), TEXT("산"), TEXT("삿"), TEXT("net"), nullptr
	};

	static const TCHAR* G_SwordContains[] = {
		TEXT("검"), TEXT("sword"), TEXT("소드"), TEXT("칼"), TEXT("점퍼"), TEXT("劍"), TEXT("刀"), TEXT("검을"), nullptr
	};
	static const TCHAR* G_SwordExact[] = {
		TEXT("검"), TEXT("칼"), TEXT("건"), TEXT("거"), TEXT("금"), TEXT("겁"), TEXT("점"), TEXT("김"), TEXT("곰"), TEXT("컴"),
		TEXT("감"), TEXT("껌"), TEXT("겜"), TEXT("권"), TEXT("던"),
		TEXT("cal"), TEXT("sword"), TEXT("ken"), TEXT("gem"), TEXT("gom"), TEXT("geom"), TEXT("com"), TEXT("gum"), nullptr
	};

	static const TCHAR* G_BowContains[] = {
		TEXT("활"), TEXT("bow"), TEXT("궁"), TEXT("궁술"), TEXT("활시위"), TEXT("활쏘"), TEXT("활을"), nullptr
	};
	static const TCHAR* G_BowExact[] = {
		TEXT("활"), TEXT("팔"), TEXT("괄"), TEXT("곽"), TEXT("화"), TEXT("발"), TEXT("핀"), TEXT("홧"), TEXT("활"),
		TEXT("bow"), TEXT("궈"), TEXT("궁"), TEXT("활이"), TEXT("팔"), TEXT("발"), nullptr
	};

	struct FWeaponRule
	{
		ENovaVoiceCommand Command;
		const TCHAR* const* ContainsKeywords;
		const TCHAR* const* ExactAliases;
	};

	static const FWeaponRule G_WeaponRules[] = {
		{ENovaVoiceCommand::Spear, G_SpearContains, G_SpearExact},
		{ENovaVoiceCommand::Shield, G_ShieldContains, G_ShieldExact},
		{ENovaVoiceCommand::Hammer, G_SwordContains, G_SwordExact},
		{ENovaVoiceCommand::Bow, G_BowContains, G_BowExact},
	};

	bool MatchesAnyKeyword(const FString& NormalizedText, const TCHAR* const* Keywords, bool bExactMatch)
	{
		if (!Keywords)
		{
			return false;
		}

		for (int32 Index = 0; Keywords[Index] != nullptr; ++Index)
		{
			const FString Keyword(Keywords[Index]);
			if (bExactMatch)
			{
				if (NormalizedText.Equals(Keyword))
				{
					return true;
				}
			}
			else if (NormalizedText.Contains(Keyword))
			{
				return true;
			}
		}

		return false;
	}

	ENovaVoiceCommand MatchWeaponRules(const FString& Normalized, const FString& HangulOnly)
	{
		for (const FWeaponRule& Rule : G_WeaponRules)
		{
			if (MatchesAnyKeyword(Normalized, Rule.ContainsKeywords, false)
				|| MatchesAnyKeyword(HangulOnly, Rule.ContainsKeywords, false))
			{
				return Rule.Command;
			}
		}

		for (const FWeaponRule& Rule : G_WeaponRules)
		{
			if (MatchesAnyKeyword(Normalized, Rule.ExactAliases, true)
				|| MatchesAnyKeyword(HangulOnly, Rule.ExactAliases, true))
			{
				return Rule.Command;
			}
		}

		return ENovaVoiceCommand::None;
	}

	bool IsExactSwordHangul(const FString& HangulOnly)
	{
		return HangulOnly == TEXT("검") || HangulOnly == TEXT("칼");
	}

	bool IsExactSpearHangul(const FString& HangulOnly)
	{
		return HangulOnly == TEXT("창") || HangulOnly == TEXT("챙");
	}

	bool IsLegacySpearMisrecognition(const FString& HangulOnly)
	{
		return HangulOnly == TEXT("낫") || HangulOnly == TEXT("나") || HangulOnly == TEXT("낮") || HangulOnly == TEXT("낳")
			|| HangulOnly == TEXT("닷") || HangulOnly == TEXT("넛") || HangulOnly == TEXT("랫") || HangulOnly == TEXT("낧")
			|| HangulOnly == TEXT("낟") || HangulOnly == TEXT("났") || HangulOnly == TEXT("냇") || HangulOnly == TEXT("산")
			|| HangulOnly == TEXT("삿") || HangulOnly == TEXT("장") || HangulOnly == TEXT("상");
	}

	struct FHangulJamo
	{
		int32 Leading = -1;
		int32 Vowel = -1;
		int32 Trailing = 0;

		bool IsValid() const { return Leading >= 0 && Vowel >= 0; }
	};

	FHangulJamo DecomposeHangulSyllable(const TCHAR Syllable)
	{
		FHangulJamo Jamo;
		if (Syllable < 0xAC00 || Syllable > 0xD7A3)
		{
			return Jamo;
		}

		const int32 SyllableIndex = Syllable - 0xAC00;
		Jamo.Leading = SyllableIndex / 588;
		Jamo.Vowel = (SyllableIndex % 588) / 28;
		Jamo.Trailing = SyllableIndex % 28;
		return Jamo;
	}

	float ScoreSpearJamo(const FHangulJamo& Jamo)
	{
		if (!Jamo.IsValid())
		{
			return 0.0f;
		}

		float Score = 0.0f;
		if (Jamo.Vowel == 0)
		{
			Score += 0.42f;
		}
		else if (Jamo.Vowel == 4 || Jamo.Vowel == 2)
		{
			Score += 0.15f;
		}
		else
		{
			return 0.0f;
		}

		switch (Jamo.Leading)
		{
		case 14: Score += 0.45f; break; // ㅊ 창
		case 12: Score += 0.38f; break; // ㅈ 장
		case 9: Score += 0.34f; break;  // ㅅ 산/상
		case 2: Score += 0.32f; break;  // ㄴ 나
		case 3: Score += 0.28f; break;  // ㄷ
		default: return 0.0f;
		}

		if (Jamo.Trailing == 21)
		{
			Score += 0.28f;
		} // ㅇ
		else if (Jamo.Trailing == 19 || Jamo.Trailing == 0)
		{
			Score += 0.22f;
		} // ㅅ/없음 (낫)
		else if (Jamo.Trailing == 4)
		{
			Score += 0.18f;
		} // ㄴ

		return Score;
	}

	float ScoreSwordJamo(const FHangulJamo& Jamo)
	{
		if (!Jamo.IsValid())
		{
			return 0.0f;
		}

		float Score = 0.0f;
		if (Jamo.Vowel == 4)
		{
			Score += 0.42f;
		} // ㅕ
		else if (Jamo.Vowel == 2 || Jamo.Vowel == 3)
		{
			Score += 0.28f;
		} // ㅓ ㅔ
		else if (Jamo.Vowel == 13 || Jamo.Vowel == 14)
		{
			Score += 0.22f;
		} // ㅡ ㅜ
		else
		{
			return 0.0f;
		}

		if (Jamo.Leading == 0 || Jamo.Leading == 1)
		{
			Score += 0.38f;
		} // ㄱ
		else if (Jamo.Leading == 12)
		{
			Score += 0.32f;
		} // ㅈ
		else if (Jamo.Leading == 13)
		{
			Score += 0.24f;
		} // ㅊ
		else
		{
			return 0.0f;
		}

		if (Jamo.Trailing == 16 || Jamo.Trailing == 0)
		{
			Score += 0.25f;
		} // ㅁ

		return Score;
	}

	float ScoreBowJamo(const FHangulJamo& Jamo)
	{
		if (!Jamo.IsValid())
		{
			return 0.0f;
		}

		float Score = 0.0f;
		if (Jamo.Vowel == 9)
		{
			Score += 0.45f;
		} // ㅘ
		else if (Jamo.Vowel == 0)
		{
			Score += 0.30f;
		} // ㅏ
		else
		{
			return 0.0f;
		}

		if (Jamo.Leading == 18)
		{
			Score += 0.40f;
		} // ㅎ
		else if (Jamo.Leading == 17)
		{
			Score += 0.32f;
		} // ㅍ
		else if (Jamo.Leading == 7)
		{
			Score += 0.28f;
		} // ㅂ
		else
		{
			return 0.0f;
		}

		if (Jamo.Trailing == 5 || Jamo.Trailing == 0)
		{
			Score += 0.22f;
		} // ㄹ

		return Score;
	}

	float ScoreShieldJamo(const FHangulJamo& Jamo)
	{
		if (!Jamo.IsValid())
		{
			return 0.0f;
		}

		float Score = 0.0f;
		if (Jamo.Vowel == 0)
		{
			Score += 0.40f;
		} // ㅏ
		else if (Jamo.Vowel == 1)
		{
			Score += 0.28f;
		} // ㅐ
		else
		{
			return 0.0f;
		}

		if (Jamo.Leading == 7)
		{
			Score += 0.38f;
		} // ㅂ 방
		else if (Jamo.Leading == 6)
		{
			Score += 0.30f;
		} // ㅁ
		else if (Jamo.Leading == 5)
		{
			Score += 0.28f;
		} // ㅂ 반
		else
		{
			return 0.0f;
		}

		if (Jamo.Trailing == 21 || Jamo.Trailing == 0)
		{
			Score += 0.22f;
		}

		return Score;
	}

	ENovaVoiceCommand MatchFuzzyHangulWeapon(const FString& HangulOnly)
	{
		if (HangulOnly.IsEmpty())
		{
			return ENovaVoiceCommand::None;
		}

		if (HangulOnly.Contains(TEXT("방패")))
		{
			return ENovaVoiceCommand::Shield;
		}

		const TCHAR Syllable = HangulOnly[HangulOnly.Len() - 1];
		const FHangulJamo Jamo = DecomposeHangulSyllable(Syllable);

		struct FWeaponScore
		{
			ENovaVoiceCommand Command;
			float Score;
		};

		const FWeaponScore Scores[] = {
			{ENovaVoiceCommand::Spear, ScoreSpearJamo(Jamo)},
			{ENovaVoiceCommand::Hammer, ScoreSwordJamo(Jamo)},
			{ENovaVoiceCommand::Bow, ScoreBowJamo(Jamo)},
			{ENovaVoiceCommand::Shield, ScoreShieldJamo(Jamo)},
		};

		ENovaVoiceCommand BestCommand = ENovaVoiceCommand::None;
		float BestScore = 0.55f;
		for (const FWeaponScore& Entry : Scores)
		{
			if (Entry.Score > BestScore)
			{
				BestScore = Entry.Score;
				BestCommand = Entry.Command;
			}
		}

		return BestCommand;
	}

	TCHAR MapCompatibilityLeadingJamo(TCHAR Character)
	{
		switch (Character)
		{
		case 0x3131: return 0x1100;
		case 0x3132: return 0x1101;
		case 0x3134: return 0x1102;
		case 0x3137: return 0x1103;
		case 0x3138: return 0x1104;
		case 0x3139: return 0x1105;
		case 0x313A: return 0x1106;
		case 0x313B: return 0x1107;
		case 0x313C: return 0x1108;
		case 0x313D: return 0x1109;
		case 0x313E: return 0x110A;
		case 0x313F: return 0x110B;
		case 0x3140: return 0x110C;
		case 0x3141: return 0x110D;
		case 0x3142: return 0x110E;
		case 0x3143: return 0x110F;
		case 0x3144: return 0x1110;
		case 0x3145: return 0x1111;
		default: return 0;
		}
	}

	FString ExpandCompatibilityJamo(const FString& In)
	{
		FString Out;
		for (const TCHAR Character : In)
		{
			if (const TCHAR Leading = MapCompatibilityLeadingJamo(Character); Leading != 0)
			{
				Out.AppendChar(Leading);
				continue;
			}

			if (Character >= 0x314F && Character <= 0x3163)
			{
				Out.AppendChar(static_cast<TCHAR>(0x1161 + (Character - 0x314F)));
				continue;
			}

			if (Character >= 0x3165 && Character <= 0x3186)
			{
				Out.AppendChar(static_cast<TCHAR>(0x11A7 + (Character - 0x3165)));
				continue;
			}

			Out.AppendChar(Character);
		}

		return Out;
	}

	bool IsSwordByHangulDecomposition(const FString& HangulOnly)
	{
		if (HangulOnly.Len() != 1)
		{
			return false;
		}

		const TCHAR Syllable = HangulOnly[0];
		if (Syllable < 0xAC00 || Syllable > 0xD7A3)
		{
			return false;
		}

		const int32 SyllableIndex = Syllable - 0xAC00;
		const int32 LeadingIndex = SyllableIndex / 588;
		const int32 VowelIndex = (SyllableIndex % 588) / 28;
		return LeadingIndex == 0 && VowelIndex == 4;
	}

	FString ComposeHangulText(const FString& In)
	{
		const FString Expanded = ExpandCompatibilityJamo(In);
		FString Out;
		for (int32 Index = 0; Index < Expanded.Len();)
		{
			const TCHAR Character = Expanded[Index];
			if (Character >= 0xAC00 && Character <= 0xD7A3)
			{
				Out.AppendChar(Character);
				++Index;
				continue;
			}

			if (Character >= 0x1100 && Character <= 0x1112 && Index + 1 < In.Len())
			{
				const TCHAR Vowel = In[Index + 1];
				if (Vowel >= 0x1161 && Vowel <= 0x1175)
				{
					const int32 LeadingIndex = Character - 0x1100;
					const int32 VowelIndex = Vowel - 0x1161;
					int32 TrailingIndex = 0;
					int32 NextIndex = Index + 2;
					if (NextIndex < In.Len())
					{
						const TCHAR Trailing = In[NextIndex];
						if (Trailing >= 0x11A8 && Trailing <= 0x11C2)
						{
							TrailingIndex = Trailing - 0x11A7;
							++NextIndex;
						}
					}

					const int32 SyllableIndex = LeadingIndex * 588 + VowelIndex * 28 + TrailingIndex;
					Out.AppendChar(static_cast<TCHAR>(0xAC00 + SyllableIndex));
					Index = NextIndex;
					continue;
				}
			}

			Out.AppendChar(Character);
			++Index;
		}

		return Out;
	}
}

FString FNovaVoiceWeaponLexicon::ExtractHangul(const FString& In)
{
	const FString Composed = ComposeHangulText(In);
	FString Out;
	for (const TCHAR Character : Composed)
	{
		if (Character >= 0xAC00 && Character <= 0xD7A3)
		{
			Out.AppendChar(Character);
		}
	}
	return Out;
}

FString FNovaVoiceWeaponLexicon::NormalizeRecognizedText(const FString& InText)
{
	FString Normalized = InText;
	Normalized.TrimStartAndEndInline();
	Normalized = Normalized.ToLower();
	Normalized.ReplaceInline(TEXT(" "), TEXT(""));
	Normalized.ReplaceInline(TEXT("."), TEXT(""));
	Normalized.ReplaceInline(TEXT("\uFF0E"), TEXT(""));
	Normalized.ReplaceInline(TEXT(","), TEXT(""));
	Normalized.ReplaceInline(TEXT("!"), TEXT(""));
	Normalized.ReplaceInline(TEXT("?"), TEXT(""));

	FString Filtered;
	for (const TCHAR Character : Normalized)
	{
		if ((Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9')))
		{
			Filtered.AppendChar(Character);
			continue;
		}

		if (Character >= 0xAC00 && Character <= 0xD7A3)
		{
			Filtered.AppendChar(Character);
			continue;
		}

		if (Character >= 0x1100 && Character <= 0x11FF)
		{
			Filtered.AppendChar(Character);
			continue;
		}

		if (Character >= 0x3130 && Character <= 0x318F)
		{
			Filtered.AppendChar(Character);
			continue;
		}

		if (Character == 0x528D || Character == 0x5200)
		{
			Filtered.AppendChar(Character);
		}
	}
	Normalized = Filtered;

	static const TCHAR* Suffixes[] = {
		TEXT("으로"), TEXT("를"), TEXT("을"), TEXT("가"), TEXT("는"), TEXT("요"), TEXT("야"), TEXT("에"), TEXT("의")
	};
	for (const TCHAR* Suffix : Suffixes)
	{
		if (Normalized.Len() > 2 && Normalized.EndsWith(Suffix))
		{
			Normalized.LeftChopInline(FCString::Strlen(Suffix));
		}
	}

	if (Normalized.Len() > 2 && Normalized.EndsWith(TEXT("이")))
	{
		Normalized.LeftChopInline(1);
	}

	return ComposeHangulText(Normalized);
}

ENovaVoiceCommand FNovaVoiceWeaponLexicon::MatchDirectHangul(const FString& HangulOnly)
{
	if (HangulOnly.IsEmpty())
	{
		return ENovaVoiceCommand::None;
	}

	if (HangulOnly == TEXT("창") || HangulOnly == TEXT("챙") || HangulOnly.Contains(TEXT("창")))
	{
		return ENovaVoiceCommand::Spear;
	}

	if (IsLegacySpearMisrecognition(HangulOnly))
	{
		return ENovaVoiceCommand::Spear;
	}

	if (HangulOnly == TEXT("검") || HangulOnly == TEXT("칼")
		|| HangulOnly == TEXT("건") || HangulOnly == TEXT("거") || HangulOnly == TEXT("금")
		|| HangulOnly == TEXT("겁") || HangulOnly == TEXT("점") || HangulOnly == TEXT("김")
		|| HangulOnly == TEXT("곰") || HangulOnly == TEXT("컴"))
	{
		return ENovaVoiceCommand::Hammer;
	}

	if (HangulOnly == TEXT("활") || HangulOnly == TEXT("팔") || HangulOnly == TEXT("발") || HangulOnly == TEXT("화")
		|| HangulOnly == TEXT("괄") || HangulOnly == TEXT("곽") || HangulOnly == TEXT("핀"))
	{
		return ENovaVoiceCommand::Bow;
	}

	if (HangulOnly == TEXT("방") || HangulOnly == TEXT("뱅") || HangulOnly == TEXT("반") || HangulOnly == TEXT("빵")
		|| HangulOnly.Contains(TEXT("방패")))
	{
		return ENovaVoiceCommand::Shield;
	}

	if (IsSwordByHangulDecomposition(HangulOnly))
	{
		return ENovaVoiceCommand::Hammer;
	}

	if (const ENovaVoiceCommand FuzzyMatch = MatchFuzzyHangulWeapon(HangulOnly); FuzzyMatch != ENovaVoiceCommand::None)
	{
		return FuzzyMatch;
	}

	return ENovaVoiceCommand::None;
}

bool FNovaVoiceWeaponLexicon::IsSpearHangul(const FString& HangulOnly)
{
	return MatchDirectHangul(HangulOnly) == ENovaVoiceCommand::Spear
		|| IsLegacySpearMisrecognition(HangulOnly)
		|| IsExactSpearHangul(HangulOnly);
}

bool FNovaVoiceWeaponLexicon::IsSwordHangul(const FString& HangulOnly)
{
	return MatchDirectHangul(HangulOnly) == ENovaVoiceCommand::Hammer;
}

ENovaVoiceCommand FNovaVoiceWeaponLexicon::DetectFromRecognizedText(const FString& RecognizedText)
{
	const FString Normalized = NormalizeRecognizedText(RecognizedText);
	FString HangulOnly = ExtractHangul(Normalized);
	if (HangulOnly.IsEmpty())
	{
		HangulOnly = ExtractHangul(RecognizedText);
	}

	const ENovaVoiceCommand DirectMatch = MatchDirectHangul(HangulOnly);
	if (DirectMatch != ENovaVoiceCommand::None)
	{
		return DirectMatch;
	}

	return MatchWeaponRules(Normalized, HangulOnly);
}

bool FNovaVoiceWeaponLexicon::IsWeaponCommand(ENovaVoiceCommand Command)
{
	return Command == ENovaVoiceCommand::Bow
		|| Command == ENovaVoiceCommand::Shield
		|| Command == ENovaVoiceCommand::Spear
		|| Command == ENovaVoiceCommand::Hammer;
}

float FNovaVoiceWeaponLexicon::GetMinConfidenceFor(
	ENovaVoiceCommand Command,
	const FString& HangulOnly,
	float DefaultMinConfidence,
	float WeaponMinConfidence)
{
	if (!IsWeaponCommand(Command))
	{
		return DefaultMinConfidence;
	}

	if (IsWeaponCommand(Command))
	{
		return 0.0f;
	}

	return WeaponMinConfidence;
}

FString FNovaVoiceWeaponLexicon::GetWeaponDisplayName(ENovaVoiceCommand Command)
{
	switch (Command)
	{
	case ENovaVoiceCommand::Hammer: return TEXT("검");
	case ENovaVoiceCommand::Bow: return TEXT("활");
	case ENovaVoiceCommand::Spear: return TEXT("창");
	case ENovaVoiceCommand::Shield: return TEXT("방패");
	default: return TEXT("?");
	}
}

bool FNovaVoiceWeaponLexicon::ResolveWeaponRecognitionText(
	const TArray<TPair<FString, float>>& NBestCandidates,
	FString& OutText,
	float& OutConfidence)
{
	if (NBestCandidates.Num() == 0)
	{
		return false;
	}

	const FString& PrimaryText = NBestCandidates[0].Key;
	const float PrimaryConfidence = NBestCandidates[0].Value;
	const FString PrimaryHangul = ExtractHangul(NormalizeRecognizedText(PrimaryText));
	const ENovaVoiceCommand PrimaryDirect = MatchDirectHangul(PrimaryHangul);

	if (PrimaryDirect != ENovaVoiceCommand::None)
	{
		if (PrimaryDirect == ENovaVoiceCommand::Spear)
		{
			for (const TPair<FString, float>& Candidate : NBestCandidates)
			{
				const FString CandidateHangul = ExtractHangul(NormalizeRecognizedText(Candidate.Key));
				if (CandidateHangul == TEXT("창") || CandidateHangul.Contains(TEXT("창")))
				{
					OutText = Candidate.Key;
					OutConfidence = FMath::Max(Candidate.Value, 0.55f);
					return true;
				}
			}

			OutText = PrimaryText;
			OutConfidence = FMath::Max(PrimaryConfidence, 0.45f);
			return true;
		}

		if (PrimaryDirect == ENovaVoiceCommand::Hammer && !IsExactSwordHangul(PrimaryHangul))
		{
			for (const TPair<FString, float>& Candidate : NBestCandidates)
			{
				const FString CandidateHangul = ExtractHangul(NormalizeRecognizedText(Candidate.Key));
				if (IsExactSwordHangul(CandidateHangul))
				{
					OutText = Candidate.Key;
					OutConfidence = Candidate.Value;
					return true;
				}
			}
		}

		OutText = PrimaryText;
		OutConfidence = PrimaryConfidence;
		return true;
	}

	ENovaVoiceCommand BestWeaponCommand = ENovaVoiceCommand::None;
	FString BestWeaponText;
	float BestWeaponConfidence = -1.0f;

	for (const TPair<FString, float>& Candidate : NBestCandidates)
	{
		const ENovaVoiceCommand WeaponCommand = DetectFromRecognizedText(Candidate.Key);
		if (WeaponCommand == ENovaVoiceCommand::None)
		{
			continue;
		}

		const FString CandidateHangul = ExtractHangul(NormalizeRecognizedText(Candidate.Key));
		const bool bDirectHangulMatch = MatchDirectHangul(CandidateHangul) == WeaponCommand;
		const bool bFuzzyHangulMatch = MatchFuzzyHangulWeapon(CandidateHangul) == WeaponCommand;
		const float RankedConfidence = Candidate.Value
			+ (bDirectHangulMatch ? 0.30f : 0.0f)
			+ (bFuzzyHangulMatch ? 0.15f : 0.0f);

		if (BestWeaponCommand == ENovaVoiceCommand::None || RankedConfidence > BestWeaponConfidence)
		{
			BestWeaponCommand = WeaponCommand;
			BestWeaponText = Candidate.Key;
			BestWeaponConfidence = RankedConfidence;
		}
	}

	if (BestWeaponCommand != ENovaVoiceCommand::None)
	{
		for (const TPair<FString, float>& Candidate : NBestCandidates)
		{
			if (Candidate.Key == BestWeaponText)
			{
				OutText = Candidate.Key;
				OutConfidence = Candidate.Value;
				return true;
			}
		}
	}

	OutText = PrimaryText;
	OutConfidence = PrimaryConfidence;
	return true;
}
