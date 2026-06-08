#pragma once

#include "AudioCaptureCore.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NovaVoiceTypes.h"
#include "NovaVoiceCaptureComponent.generated.h"

class UNovaAzureSpeechClient;
class UNovaVoiceCommandParser;

UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaVoiceCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaVoiceCaptureComponent();
	virtual ~UNovaVoiceCaptureComponent() override;

	UPROPERTY(BlueprintAssignable, Category = "Voice")
	FNovaVoiceCommandDelegate OnVoiceCommandRecognized;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Azure")
	FString AzureRegion = TEXT("koreacentral");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Azure")
	FString AzureLanguage = TEXT("ko-KR");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Capture")
	bool bAutoStartListening = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Capture")
	int32 TargetSampleRate = 16000;

	/** Partial match against Windows capture device name (e.g. "Hands-Free"). Loaded from LocalNovaVoice.ini if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Capture")
	FString CaptureDeviceName;

	/** If opening the preferred device fails, try other capture devices (Hands-Free first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Capture")
	bool bRetryAllCaptureDevices = true;

	UPROPERTY(BlueprintReadOnly, Category = "Voice|Debug")
	FString ActiveCaptureDeviceName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|VAD")
	float SpeechStartThreshold = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|VAD")
	float SpeechContinueThreshold = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|VAD")
	float SilenceSecondsToFinalize = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|VAD")
	float MinUtteranceSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|VAD")
	float MaxUtteranceSeconds = 2.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Voice|Debug")
	FNovaVoiceLatencyStats LatencyStats;

	UFUNCTION(BlueprintCallable, Category = "Voice")
	bool StartListening();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	void StopListening();

	UFUNCTION(BlueprintCallable, Category = "Voice")
	bool IsListening() const { return bIsListening; }

	UFUNCTION(BlueprintCallable, Category = "Voice|Debug")
	FString GetDebugSummary() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EVoiceVadState : uint8
	{
		Idle,
		Speaking,
		Finalizing
	};

	UPROPERTY()
	TObjectPtr<UNovaAzureSpeechClient> SpeechClient;

	UPROPERTY()
	TObjectPtr<UNovaVoiceCommandParser> CommandParser;

	TUniquePtr<Audio::FAudioCapture> AudioCapture;
	FCriticalSection AudioMutex;
	TArray<float> PendingFloatSamples;
	int32 CaptureSampleRate = 16000;
	int32 CaptureNumChannels = 1;

	bool bIsListening = false;
	bool bIsRecognizing = false;
	EVoiceVadState VadState = EVoiceVadState::Idle;
	TArray<float> UtteranceBuffer;
	float SilenceTimer = 0.0f;
	double UtteranceStartSeconds = 0.0;
	double LastRecognitionRequestStartSeconds = 0.0;

	FString ResolveSubscriptionKey() const;
	bool OpenCaptureWithFallback();
	bool TryOpenCaptureStream(int32 DeviceIndex, int32 SampleRate, int32 NumChannels, FString& OutOpenedDeviceName);
	static TArray<Audio::FCaptureDeviceInfo> QueryCaptureDevices();
	static bool DeviceNameMatchesFilter(const FString& DeviceName, const FString& Filter);
	static int32 ScoreCaptureDevicePriority(const FString& DeviceName);
	static FString BuildCaptureDevicesHelpText(const TArray<Audio::FCaptureDeviceInfo>& Devices);
	void OnAudioCaptured(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, bool bOverflow);
	void ProcessVad(float DeltaTime);
	void FinalizeUtterance();
	void HandleAzureResult(bool bSuccess, const FString& Text, float Confidence);
	void UpdateLatencyStats(bool bSuccess, float RoundTripMs);
	void BroadcastDebug(const FString& Message, FColor Color = FColor::Yellow) const;
	static void ResampleToTargetRate(const TArray<float>& InSamples, int32 InRate, int32 OutRate, TArray<int16>& OutPcm16);
};
