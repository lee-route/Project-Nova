#include "NovaAzureSpeechClient.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UNovaAzureSpeechClient::SetSubscriptionKey(const FString& InKey)
{
	SubscriptionKey = InKey;
}

bool UNovaAzureSpeechClient::HasValidCredentials() const
{
	return !SubscriptionKey.IsEmpty() && !Region.IsEmpty();
}

TArray<uint8> UNovaAzureSpeechClient::BuildWavFromPcm16(const TArray<int16>& PcmSamples, int32 SampleRate)
{
	TArray<uint8> WavData;
	const int32 NumChannels = 1;
	const int32 BitsPerSample = 16;
	const int32 ByteRate = SampleRate * NumChannels * BitsPerSample / 8;
	const int32 BlockAlign = NumChannels * BitsPerSample / 8;
	const int32 DataSize = PcmSamples.Num() * sizeof(int16);
	const int32 ChunkSize = 36 + DataSize;

	auto AppendBytes = [&WavData](const void* Data, int32 Size)
	{
		const int32 Offset = WavData.Num();
		WavData.AddUninitialized(Size);
		FMemory::Memcpy(WavData.GetData() + Offset, Data, Size);
	};

	auto AppendInt32 = [&AppendBytes](int32 Value)
	{
		AppendBytes(&Value, sizeof(int32));
	};

	auto AppendInt16 = [&AppendBytes](int16 Value)
	{
		AppendBytes(&Value, sizeof(int16));
	};

	const uint8 Riff[4] = {'R', 'I', 'F', 'F'};
	AppendBytes(Riff, 4);
	AppendInt32(ChunkSize);

	const uint8 Wave[4] = {'W', 'A', 'V', 'E'};
	AppendBytes(Wave, 4);

	const uint8 Fmt[4] = {'f', 'm', 't', ' '};
	AppendBytes(Fmt, 4);
	AppendInt32(16);

	const int16 AudioFormat = 1;
	AppendInt16(AudioFormat);
	AppendInt16(static_cast<int16>(NumChannels));
	AppendInt32(SampleRate);
	AppendInt32(ByteRate);
	AppendInt16(static_cast<int16>(BlockAlign));
	AppendInt16(static_cast<int16>(BitsPerSample));

	const uint8 Data[4] = {'d', 'a', 't', 'a'};
	AppendBytes(Data, 4);
	AppendInt32(DataSize);
	AppendBytes(PcmSamples.GetData(), DataSize);

	return WavData;
}

void UNovaAzureSpeechClient::RecognizePcm16Mono(const TArray<int16>& PcmSamples, int32 SampleRate, FNovaAzureSpeechResultDelegate OnComplete)
{
	if (!HasValidCredentials())
	{
		OnComplete.ExecuteIfBound(false, TEXT("Azure Speech key/region missing"), 0.0f);
		return;
	}

	if (PcmSamples.Num() == 0)
	{
		OnComplete.ExecuteIfBound(false, TEXT("Empty audio buffer"), 0.0f);
		return;
	}

	const TArray<uint8> WavData = BuildWavFromPcm16(PcmSamples, SampleRate);
	const FString Url = FString::Printf(
		TEXT("https://%s.stt.speech.microsoft.com/speech/recognition/conversation/cognitiveservices/v1?language=%s&format=detailed"),
		*Region,
		*Language
	);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Ocp-Apim-Subscription-Key"), SubscriptionKey);
	Request->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("audio/wav; codecs=audio/pcm; samplerate=%d"), SampleRate));
	Request->SetContent(WavData);
	Request->SetTimeout(RequestTimeoutSeconds);

	PendingResultDelegate = OnComplete;
	Request->OnProcessRequestComplete().BindUObject(this, &UNovaAzureSpeechClient::HandleRecognitionResponse);
	Request->ProcessRequest();
}

void UNovaAzureSpeechClient::HandleRecognitionResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		PendingResultDelegate.ExecuteIfBound(false, TEXT("HTTP request failed"), 0.0f);
		PendingResultDelegate.Unbind();
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		PendingResultDelegate.ExecuteIfBound(
			false,
			FString::Printf(TEXT("Azure HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString()),
			0.0f
		);
		PendingResultDelegate.Unbind();
		return;
	}

	const FString Body = Response->GetContentAsString();
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		PendingResultDelegate.ExecuteIfBound(false, TEXT("Invalid Azure JSON response"), 0.0f);
		PendingResultDelegate.Unbind();
		return;
	}

	FString Status;
	if (!JsonObject->TryGetStringField(TEXT("RecognitionStatus"), Status) || Status != TEXT("Success"))
	{
		PendingResultDelegate.ExecuteIfBound(false, FString::Printf(TEXT("RecognitionStatus=%s"), *Status), 0.0f);
		PendingResultDelegate.Unbind();
		return;
	}

	FString DisplayText;
	JsonObject->TryGetStringField(TEXT("DisplayText"), DisplayText);

	float Confidence = 0.0f;
	const TArray<TSharedPtr<FJsonValue>>* NBestArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("NBest"), NBestArray) && NBestArray && NBestArray->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* BestObject = nullptr;
		if ((*NBestArray)[0]->TryGetObject(BestObject) && BestObject && BestObject->IsValid())
		{
			(*BestObject)->TryGetNumberField(TEXT("Confidence"), Confidence);
			if (DisplayText.IsEmpty())
			{
				(*BestObject)->TryGetStringField(TEXT("Display"), DisplayText);
			}
		}
	}

	if (DisplayText.IsEmpty())
	{
		PendingResultDelegate.ExecuteIfBound(false, TEXT("Empty recognition text"), Confidence);
		PendingResultDelegate.Unbind();
		return;
	}

	PendingResultDelegate.ExecuteIfBound(true, DisplayText, Confidence);
	PendingResultDelegate.Unbind();
}
