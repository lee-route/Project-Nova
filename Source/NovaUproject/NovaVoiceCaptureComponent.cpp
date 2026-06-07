#include "NovaVoiceCaptureComponent.h"

#include "AudioCaptureCore.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NovaAzureSpeechClient.h"
#include "NovaVoiceCommandParser.h"
#include "NovaVoiceWeaponLexicon.h"

UNovaVoiceCaptureComponent::UNovaVoiceCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UNovaVoiceCaptureComponent::~UNovaVoiceCaptureComponent()
{
	StopListening();
}

void UNovaVoiceCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	SpeechClient = NewObject<UNovaAzureSpeechClient>(this);
	CommandParser = NewObject<UNovaVoiceCommandParser>(this);

	auto LoadString = [](const TCHAR* Key, FString& OutValue, const FString& ExtraIniPath = FString())
	{
		FString Loaded = OutValue;
		if (GConfig->GetString(TEXT("/Script/NovaUproject.NovaVoiceSettings"), Key, Loaded, GGameIni))
		{
			OutValue = Loaded;
		}

		if (!ExtraIniPath.IsEmpty() && FPaths::FileExists(ExtraIniPath))
		{
			if (GConfig->GetString(TEXT("/Script/NovaUproject.NovaVoiceSettings"), Key, Loaded, ExtraIniPath))
			{
				OutValue = Loaded;
			}
		}
	};

	auto LoadFloat = [](const TCHAR* Key, float& OutValue, const FString& ExtraIniPath = FString())
	{
		float Loaded = OutValue;
		if (GConfig->GetFloat(TEXT("/Script/NovaUproject.NovaVoiceSettings"), Key, Loaded, GGameIni))
		{
			OutValue = Loaded;
		}

		if (!ExtraIniPath.IsEmpty() && FPaths::FileExists(ExtraIniPath))
		{
			if (GConfig->GetFloat(TEXT("/Script/NovaUproject.NovaVoiceSettings"), Key, Loaded, ExtraIniPath))
			{
				OutValue = Loaded;
			}
		}
	};

	FString LocalConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("LocalNovaVoice.ini"));
	FConfigCacheIni::NormalizeConfigIniPath(LocalConfigPath);
	LoadString(TEXT("AzureRegion"), AzureRegion, LocalConfigPath);
	LoadString(TEXT("AzureLanguage"), AzureLanguage, LocalConfigPath);
	LoadString(TEXT("CaptureDeviceName"), CaptureDeviceName, LocalConfigPath);

	if (SpeechClient)
	{
		SpeechClient->Region = AzureRegion;
		SpeechClient->Language = AzureLanguage;
		SpeechClient->SetSubscriptionKey(ResolveSubscriptionKey());
	}

	if (CommandParser)
	{
		LoadFloat(TEXT("MinCommandConfidence"), CommandParser->MinConfidence, LocalConfigPath);
		LoadFloat(TEXT("MinWeaponCommandConfidence"), CommandParser->MinWeaponConfidence, LocalConfigPath);
	}

	if (bAutoStartListening)
	{
		StartListening();
	}
}

void UNovaVoiceCaptureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopListening();
	Super::EndPlay(EndPlayReason);
}

FString UNovaVoiceCaptureComponent::ResolveSubscriptionKey() const
{
	const FString EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("NOVA_AZURE_SPEECH_KEY"));
	if (!EnvKey.IsEmpty())
	{
		return EnvKey;
	}

	const FString LocalConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("LocalNovaVoice.ini"));
	if (FPaths::FileExists(LocalConfigPath))
	{
		FString Key;
		if (GConfig->GetString(TEXT("/Script/NovaUproject.NovaVoiceSettings"), TEXT("AzureSubscriptionKey"), Key, LocalConfigPath)
			&& !Key.IsEmpty())
		{
			return Key;
		}
	}

	return FString();
}

bool UNovaVoiceCaptureComponent::StartListening()
{
	if (bIsListening)
	{
		return true;
	}

	if (!SpeechClient || !SpeechClient->HasValidCredentials())
	{
		BroadcastDebug(TEXT("Voice: Azure key missing. Set NOVA_AZURE_SPEECH_KEY or Config/LocalNovaVoice.ini"), FColor::Red);
		return false;
	}

	if (OpenCaptureWithFallback())
	{
		bIsListening = true;
		VadState = EVoiceVadState::Idle;
		UtteranceBuffer.Reset();
		SilenceTimer = 0.0f;

		BroadcastDebug(
			FString::Printf(
				TEXT("Voice listening: %s (%d Hz capture -> %d Hz STT, %d ch)"),
				*ActiveCaptureDeviceName,
				CaptureSampleRate,
				TargetSampleRate,
				CaptureNumChannels),
			FColor::Green);
		return true;
	}

	const TArray<Audio::FCaptureDeviceInfo> Devices = QueryCaptureDevices();
	const FString HelpText = BuildCaptureDevicesHelpText(Devices);
	BroadcastDebug(
		FString::Printf(
			TEXT("Voice: failed to open microphone. Set Windows input to Hands-Free/AG Audio, or CaptureDeviceName in LocalNovaVoice.ini. Devices: %s"),
			*HelpText),
		FColor::Red);
	return false;
}

TArray<Audio::FCaptureDeviceInfo> UNovaVoiceCaptureComponent::QueryCaptureDevices()
{
	TArray<Audio::FCaptureDeviceInfo> Devices;
	Audio::FAudioCapture Probe;
	Probe.GetCaptureDevicesAvailable(Devices);
	return Devices;
}

bool UNovaVoiceCaptureComponent::DeviceNameMatchesFilter(const FString& DeviceName, const FString& Filter)
{
	if (Filter.IsEmpty())
	{
		return false;
	}

	return DeviceName.Contains(Filter, ESearchCase::IgnoreCase);
}

int32 UNovaVoiceCaptureComponent::ScoreCaptureDevicePriority(const FString& DeviceName)
{
	const FString Lower = DeviceName.ToLower();
	int32 Score = 0;

	if (Lower.Contains(TEXT("hands-free")) || Lower.Contains(TEXT("hand-free")) || Lower.Contains(TEXT("ag audio")))
	{
		Score += 100;
	}

	if (Lower.Contains(TEXT("headset")) || Lower.Contains(TEXT("microphone")) || Lower.Contains(TEXT(" mic")))
	{
		Score += 40;
	}

	if (Lower.Contains(TEXT("stereo")) && !Lower.Contains(TEXT("hands-free")) && !Lower.Contains(TEXT("hand-free")))
	{
		Score -= 60;
	}

	return Score;
}

FString UNovaVoiceCaptureComponent::BuildCaptureDevicesHelpText(const TArray<Audio::FCaptureDeviceInfo>& Devices)
{
	if (Devices.Num() == 0)
	{
		return TEXT("(none detected)");
	}

	FString HelpText;
	for (int32 Index = 0; Index < Devices.Num(); ++Index)
	{
		if (!HelpText.IsEmpty())
		{
			HelpText += TEXT(" | ");
		}

		HelpText += FString::Printf(
			TEXT("[%d] %s (%d Hz)"),
			Index,
			*Devices[Index].DeviceName,
			Devices[Index].PreferredSampleRate);
	}

	return HelpText;
}

bool UNovaVoiceCaptureComponent::TryOpenCaptureStream(
	int32 DeviceIndex,
	int32 SampleRate,
	int32 NumChannels,
	FString& OutOpenedDeviceName)
{
	AudioCapture = MakeUnique<Audio::FAudioCapture>();

	Audio::FAudioCaptureDeviceParams DeviceParams;
	DeviceParams.DeviceIndex = DeviceIndex;
	DeviceParams.SampleRate = SampleRate;
	DeviceParams.NumInputChannels = NumChannels;

	const uint32 BufferFrames = 4096;
	const bool bOpened = AudioCapture->OpenAudioCaptureStream(
		DeviceParams,
		[this](const void* InAudio, int32 NumFrames, int32 InNumChannels, int32 InSampleRate, double /*StreamTime*/, bool bOverflow)
		{
			OnAudioCaptured(InAudio, NumFrames, InNumChannels, InSampleRate, bOverflow);
		},
		BufferFrames);

	if (!bOpened)
	{
		AudioCapture.Reset();
		BroadcastDebug(TEXT("Voice: OpenAudioCaptureStream FAILED"), FColor::Red);
		return false;
	}

	if (!AudioCapture->StartStream())
	{
		AudioCapture->AbortStream();
		AudioCapture.Reset();
		BroadcastDebug(TEXT("Voice: StartStream FAILED"), FColor::Red);
		return false;
	}

	Audio::FCaptureDeviceInfo DeviceInfo;
	if (AudioCapture->GetCaptureDeviceInfo(DeviceInfo, DeviceIndex))
	{
		OutOpenedDeviceName = DeviceInfo.DeviceName;
	}
	else if (DeviceIndex == INDEX_NONE)
	{
		OutOpenedDeviceName = TEXT("Default");
	}
	else
	{
		OutOpenedDeviceName = FString::Printf(TEXT("Device %d"), DeviceIndex);
	}

	CaptureSampleRate = AudioCapture->GetSampleRate() > 0 ? AudioCapture->GetSampleRate() : TargetSampleRate;
	CaptureNumChannels = DeviceInfo.InputChannels > 0 ? DeviceInfo.InputChannels : 1;
	return true;
}

bool UNovaVoiceCaptureComponent::OpenCaptureWithFallback()
{
	const TArray<Audio::FCaptureDeviceInfo> Devices = QueryCaptureDevices();

	TArray<int32> DeviceTryOrder;
	if (!CaptureDeviceName.IsEmpty())
	{
		for (int32 Index = 0; Index < Devices.Num(); ++Index)
		{
			if (DeviceNameMatchesFilter(Devices[Index].DeviceName, CaptureDeviceName))
			{
				DeviceTryOrder.Add(Index);
			}
		}
	}

	DeviceTryOrder.Add(INDEX_NONE);

	if (bRetryAllCaptureDevices)
	{
		TArray<int32> SortedIndices;
		SortedIndices.Reserve(Devices.Num());
		for (int32 Index = 0; Index < Devices.Num(); ++Index)
		{
			SortedIndices.Add(Index);
		}

		SortedIndices.Sort([&Devices](int32 A, int32 B)
		{
			const int32 ScoreA = ScoreCaptureDevicePriority(Devices[A].DeviceName);
			const int32 ScoreB = ScoreCaptureDevicePriority(Devices[B].DeviceName);
			if (ScoreA != ScoreB)
			{
				return ScoreA > ScoreB;
			}

			return Devices[A].DeviceName < Devices[B].DeviceName;
		});

		for (const int32 Index : SortedIndices)
		{
			DeviceTryOrder.AddUnique(Index);
		}
	}

	struct FCaptureParamAttempt
	{
		int32 SampleRate = Audio::InvalidDeviceSampleRate;
		int32 NumChannels = Audio::InvalidDeviceChannelCount;
	};

	for (const int32 DeviceIndex : DeviceTryOrder)
	{
		const Audio::FCaptureDeviceInfo* DeviceInfo = nullptr;
		Audio::FCaptureDeviceInfo ResolvedDeviceInfo;
		if (DeviceIndex == INDEX_NONE)
		{
			Audio::FAudioCapture Probe;
			if (Probe.GetCaptureDeviceInfo(ResolvedDeviceInfo, INDEX_NONE))
			{
				DeviceInfo = &ResolvedDeviceInfo;
			}
		}
		else if (Devices.IsValidIndex(DeviceIndex))
		{
			DeviceInfo = &Devices[DeviceIndex];
		}

		TArray<FCaptureParamAttempt> ParamAttempts;
		ParamAttempts.Add({Audio::InvalidDeviceSampleRate, Audio::InvalidDeviceChannelCount});

		if (DeviceInfo && DeviceInfo->PreferredSampleRate > 0)
		{
			ParamAttempts.Add({DeviceInfo->PreferredSampleRate, Audio::InvalidDeviceChannelCount});
		}

		ParamAttempts.Add({48000, Audio::InvalidDeviceChannelCount});
		ParamAttempts.Add({44100, Audio::InvalidDeviceChannelCount});
		ParamAttempts.Add({16000, 1});

		for (const FCaptureParamAttempt& Attempt : ParamAttempts)
		{
			FString OpenedDeviceName;
			if (TryOpenCaptureStream(DeviceIndex, Attempt.SampleRate, Attempt.NumChannels, OpenedDeviceName))
			{
				ActiveCaptureDeviceName = OpenedDeviceName;
				return true;
			}
		}
	}

	ActiveCaptureDeviceName.Reset();
	return false;
}

void UNovaVoiceCaptureComponent::StopListening()
{
	if (AudioCapture.IsValid())
	{
		AudioCapture->StopStream();
		AudioCapture->CloseStream();
		AudioCapture.Reset();
	}

	bIsListening = false;
	VadState = EVoiceVadState::Idle;
	UtteranceBuffer.Reset();
}

void UNovaVoiceCaptureComponent::OnAudioCaptured(
	const void* InAudio,
	int32 NumFrames,
	int32 NumChannels,
	int32 SampleRate,
	bool bOverflow)
{
	if (!InAudio || NumFrames <= 0 || !bIsListening || bIsRecognizing)
	{
		return;
	}

	if (bOverflow)
	{
		UE_LOG(LogTemp, Warning, TEXT("NOVA Voice: audio capture overflow"));
	}

	const float* FloatAudio = static_cast<const float*>(InAudio);

	FScopeLock Lock(&AudioMutex);
	CaptureSampleRate = SampleRate > 0 ? SampleRate : CaptureSampleRate;
	CaptureNumChannels = NumChannels > 0 ? NumChannels : CaptureNumChannels;

	const int32 ChannelCount = FMath::Max(1, CaptureNumChannels);
	const int32 StartIndex = PendingFloatSamples.Num();
	PendingFloatSamples.AddUninitialized(NumFrames);
	for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
	{
		PendingFloatSamples[StartIndex + FrameIndex] = FloatAudio[FrameIndex * ChannelCount];
	}
}

void UNovaVoiceCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsListening || bIsRecognizing)
	{
		return;
	}

	{
		FScopeLock Lock(&AudioMutex);
		if (PendingFloatSamples.Num() > 0)
		{
			UtteranceBuffer.Append(PendingFloatSamples);
			PendingFloatSamples.Reset();
		}
	}

	ProcessVad(DeltaTime);
}

void UNovaVoiceCaptureComponent::ProcessVad(float DeltaTime)
{
	if (UtteranceBuffer.Num() == 0)
	{
		return;
	}

	const int32 WindowSize = FMath::Clamp(CaptureSampleRate / 20, 256, 2048);
	const int32 Start = FMath::Max(0, UtteranceBuffer.Num() - WindowSize);
	float SumSquares = 0.0f;
	for (int32 Index = Start; Index < UtteranceBuffer.Num(); ++Index)
	{
		const float Sample = UtteranceBuffer[Index];
		SumSquares += Sample * Sample;
	}
	const float Rms = FMath::Sqrt(SumSquares / static_cast<float>(UtteranceBuffer.Num() - Start));

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float UtteranceSeconds = UtteranceBuffer.Num() / static_cast<float>(CaptureSampleRate);

	switch (VadState)
	{
	case EVoiceVadState::Idle:
		if (Rms >= SpeechStartThreshold)
		{
			VadState = EVoiceVadState::Speaking;
			UtteranceStartSeconds = Now;
			SilenceTimer = 0.0f;
			BroadcastDebug(TEXT("Voice: speech detected"), FColor::Cyan);
		}
		else
		{
			UtteranceBuffer.Reset();
		}
		break;

	case EVoiceVadState::Speaking:
		if (Rms < SpeechContinueThreshold)
		{
			SilenceTimer += DeltaTime;
			if (SilenceTimer >= SilenceSecondsToFinalize && UtteranceSeconds >= MinUtteranceSeconds)
			{
				FinalizeUtterance();
			}
		}
		else
		{
			SilenceTimer = 0.0f;
		}

		if (UtteranceSeconds >= MaxUtteranceSeconds)
		{
			FinalizeUtterance();
		}
		break;

	default:
		break;
	}
}

void UNovaVoiceCaptureComponent::ResampleToTargetRate(
	const TArray<float>& InSamples,
	int32 InRate,
	int32 OutRate,
	TArray<int16>& OutPcm16)
{
	if (InSamples.Num() == 0 || InRate <= 0 || OutRate <= 0)
	{
		OutPcm16.Reset();
		return;
	}

	const double Ratio = static_cast<double>(InRate) / static_cast<double>(OutRate);
	const int32 OutCount = FMath::Max(1, static_cast<int32>(InSamples.Num() / Ratio));
	OutPcm16.SetNumUninitialized(OutCount);

	for (int32 OutIndex = 0; OutIndex < OutCount; ++OutIndex)
	{
		const int32 SrcIndex = FMath::Clamp(static_cast<int32>(OutIndex * Ratio), 0, InSamples.Num() - 1);
		const float Sample = FMath::Clamp(InSamples[SrcIndex], -1.0f, 1.0f);
		OutPcm16[OutIndex] = static_cast<int16>(Sample * 32767.0f);
	}
}

void UNovaVoiceCaptureComponent::FinalizeUtterance()
{
	if (bIsRecognizing || UtteranceBuffer.Num() == 0 || !SpeechClient)
	{
		VadState = EVoiceVadState::Idle;
		UtteranceBuffer.Reset();
		SilenceTimer = 0.0f;
		return;
	}

	TArray<float> SamplesToSend = MoveTemp(UtteranceBuffer);
	VadState = EVoiceVadState::Idle;
	SilenceTimer = 0.0f;
	bIsRecognizing = true;

	TArray<int16> Pcm16;
	ResampleToTargetRate(SamplesToSend, CaptureSampleRate, TargetSampleRate, Pcm16);

	const UWorld* World = GetWorld();
	LastRecognitionRequestStartSeconds = World ? World->GetTimeSeconds() : 0.0;
	LatencyStats.TotalRequests++;

	BroadcastDebug(FString::Printf(TEXT("Voice: sending %.2fs audio"), SamplesToSend.Num() / static_cast<float>(CaptureSampleRate)), FColor::Silver);

	SpeechClient->RecognizePcm16Mono(
		Pcm16,
		TargetSampleRate,
		FNovaAzureSpeechResultDelegate::CreateUObject(this, &UNovaVoiceCaptureComponent::HandleAzureResult)
	);
}

void UNovaVoiceCaptureComponent::HandleAzureResult(bool bSuccess, const FString& Text, float Confidence)
{
	bIsRecognizing = false;

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float RoundTripMs = static_cast<float>((Now - LastRecognitionRequestStartSeconds) * 1000.0);
	UpdateLatencyStats(bSuccess, RoundTripMs);

	if (!bSuccess)
	{
		BroadcastDebug(FString::Printf(TEXT("Voice STT fail: %s"), *Text), FColor::Red);
		return;
	}

	if (!CommandParser)
	{
		return;
	}

	const FNovaVoiceCommandResult Parsed = CommandParser->Parse(Text, Confidence);
	if (!Parsed.bAccepted)
	{
		LatencyStats.RejectedCommands++;
		const ENovaVoiceCommand ParsedCommand = Parsed.Command;
		FString RejectDetail = ParsedCommand != ENovaVoiceCommand::None
			? FString::Printf(TEXT("matched=%d"), static_cast<int32>(ParsedCommand))
			: TEXT("no match");
		if (ParsedCommand == ENovaVoiceCommand::None)
		{
			const FString DebugHangul = FNovaVoiceWeaponLexicon::ExtractHangul(
				FNovaVoiceWeaponLexicon::NormalizeRecognizedText(Text));
			if (!DebugHangul.IsEmpty())
			{
				RejectDetail = FString::Printf(TEXT("no match (hangul=%s)"), *DebugHangul);
			}
		}
		BroadcastDebug(
			FString::Printf(TEXT("Voice rejected: \"%s\" (%.2f, %s)"), *Text, Confidence, *RejectDetail),
			FColor::Orange
		);
		return;
	}

	BroadcastDebug(
		FString::Printf(
			TEXT("Voice command: %s -> %s(%d) (%.2f, %.0fms)"),
			*Text,
			*FNovaVoiceWeaponLexicon::GetWeaponDisplayName(Parsed.Command),
			static_cast<int32>(Parsed.Command),
			Confidence,
			RoundTripMs),
		FColor::Green
	);
	OnVoiceCommandRecognized.Broadcast(Parsed);
}

void UNovaVoiceCaptureComponent::UpdateLatencyStats(bool bSuccess, float RoundTripMs)
{
	if (bSuccess)
	{
		LatencyStats.SuccessfulRequests++;
	}
	else
	{
		LatencyStats.FailedRequests++;
	}

	LatencyStats.LastRoundTripMs = RoundTripMs;
	LatencyStats.MaxRoundTripMs = FMath::Max(LatencyStats.MaxRoundTripMs, RoundTripMs);

	const int32 SuccessCount = FMath::Max(1, LatencyStats.SuccessfulRequests);
	const float PreviousTotal = LatencyStats.AverageRoundTripMs * static_cast<float>(SuccessCount - 1);
	LatencyStats.AverageRoundTripMs = bSuccess
		? (PreviousTotal + RoundTripMs) / static_cast<float>(SuccessCount)
		: LatencyStats.AverageRoundTripMs;
}

FString UNovaVoiceCaptureComponent::GetDebugSummary() const
{
	const FString DeviceSummary = ActiveCaptureDeviceName.IsEmpty() ? TEXT("mic=off") : FString::Printf(TEXT("mic=%s"), *ActiveCaptureDeviceName);
	return FString::Printf(
		TEXT("%s | STT req=%d ok=%d fail=%d reject=%d last=%.0fms avg=%.0fms max=%.0fms"),
		*DeviceSummary,
		LatencyStats.TotalRequests,
		LatencyStats.SuccessfulRequests,
		LatencyStats.FailedRequests,
		LatencyStats.RejectedCommands,
		LatencyStats.LastRoundTripMs,
		LatencyStats.AverageRoundTripMs,
		LatencyStats.MaxRoundTripMs
	);
}

void UNovaVoiceCaptureComponent::BroadcastDebug(const FString& Message, FColor Color) const
{
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, Color, Message);
	}
}
