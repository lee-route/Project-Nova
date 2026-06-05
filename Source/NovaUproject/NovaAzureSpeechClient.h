#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "NovaAzureSpeechClient.generated.h"

DECLARE_DELEGATE_ThreeParams(FNovaAzureSpeechResultDelegate, bool /*bSuccess*/, const FString& /*Text*/, float /*Confidence*/);

UCLASS()
class NOVAUPROJECT_API UNovaAzureSpeechClient : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Azure")
	FString Region = TEXT("koreacentral");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Azure")
	FString Language = TEXT("ko-KR");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Azure")
	float RequestTimeoutSeconds = 4.0f;

	void SetSubscriptionKey(const FString& InKey);
	bool HasValidCredentials() const;

	void RecognizePcm16Mono(const TArray<int16>& PcmSamples, int32 SampleRate, FNovaAzureSpeechResultDelegate OnComplete);

private:
	FString SubscriptionKey;
	FNovaAzureSpeechResultDelegate PendingResultDelegate;

	static TArray<uint8> BuildWavFromPcm16(const TArray<int16>& PcmSamples, int32 SampleRate);
	void HandleRecognitionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
