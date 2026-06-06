#include "NovaVoiceCaptureComponent.h"

#include "AudioCaptureCore.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NovaAzureSpeechClient.h"
#include "NovaVoiceCommandParser.h"

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

	const FString LocalConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("LocalNovaVoice.ini"));
	LoadString(TEXT("AzureRegion"), AzureRegion, LocalConfigPath);
	LoadString(TEXT("AzureLanguage"), AzureLanguage, LocalConfigPath);

	if (SpeechClient)
	{
		SpeechClient->Region = AzureRegion;
		SpeechClient->Language = AzureLanguage;
		SpeechClient->SetSubscriptionKey(ResolveSubscriptionKey());
	}

	if (CommandParser)
	{
		LoadFloat(TEXT("MinCommandConfidence"), CommandParser->MinConfidence, LocalConfigPath);
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

	AudioCapture = MakeUnique<Audio::FAudioCapture>();
	Audio::FAudioCaptureDeviceParams DeviceParams;
	DeviceParams.SampleRate = TargetSampleRate;
	DeviceParams.NumInputChannels = 1;
	const uint32 BufferFrames = 1024;

	const bool bOpened = AudioCapture->OpenAudioCaptureStream(
		DeviceParams,
		[this](const void* InAudio, int32 NumFrames, int32 InNumChannels, int32 InSampleRate, double /*StreamTime*/, bool bOverflow)
		{
			OnAudioCaptured(static_cast<const float*>(InAudio), NumFrames, InNumChannels, InSampleRate, bOverflow);
		},
		BufferFrames
	);

	if (!bOpened || !AudioCapture->StartStream())
	{
		AudioCapture.Reset();
		BroadcastDebug(TEXT("Voice: failed to open default microphone"), FColor::Red);
		return false;
	}

	CaptureSampleRate = AudioCapture->GetSampleRate() > 0 ? AudioCapture->GetSampleRate() : TargetSampleRate;
	CaptureNumChannels = 1;
	bIsListening = true;
	VadState = EVoiceVadState::Idle;
	UtteranceBuffer.Reset();
	SilenceTimer = 0.0f;

	BroadcastDebug(FString::Printf(TEXT("Voice listening (%d Hz, %d ch)"), CaptureSampleRate, CaptureNumChannels), FColor::Green);
	return true;
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
	const float* InAudio,
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

	FScopeLock Lock(&AudioMutex);
	CaptureSampleRate = SampleRate > 0 ? SampleRate : CaptureSampleRate;
	CaptureNumChannels = NumChannels > 0 ? NumChannels : CaptureNumChannels;

	const int32 StartIndex = PendingFloatSamples.Num();
	PendingFloatSamples.AddUninitialized(NumFrames);
	for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
	{
		PendingFloatSamples[StartIndex + FrameIndex] = InAudio[FrameIndex * CaptureNumChannels];
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
		BroadcastDebug(
			FString::Printf(TEXT("Voice rejected: \"%s\" (%.2f)"), *Text, Confidence),
			FColor::Orange
		);
		return;
	}

	BroadcastDebug(
		FString::Printf(TEXT("Voice command: %s (%.2f, %.0fms)"), *Text, Confidence, RoundTripMs),
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
	return FString::Printf(
		TEXT("STT req=%d ok=%d fail=%d reject=%d last=%.0fms avg=%.0fms max=%.0fms"),
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
