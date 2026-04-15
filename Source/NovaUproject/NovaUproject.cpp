// Fill out your copyright notice in the Description page of Project Settings.

#include "NovaUproject.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "NovaGameMode.h"
#include "Misc/MessageDialog.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/UObjectGlobals.h"
#endif

namespace NovaUprojectEditorWarnings
{
#if WITH_EDITOR
	static FDelegateHandle BeginPIEHandle;
	static FDelegateHandle PostPIEStartedHandle;

	static void WarnIfGameModeOverridden(const bool bIsSimulating)
	{
		(void)bIsSimulating;

		if (!GEditor)
		{
			return;
		}

		UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			return;
		}

		AWorldSettings* WS = World->GetWorldSettings();
		if (!WS)
		{
			return;
		}

		const TSubclassOf<AGameModeBase> OverrideGM = WS->DefaultGameMode;
		if (OverrideGM && OverrideGM != ANovaGameMode::StaticClass())
		{
			const FString GMName = OverrideGM->GetName();
			const FString Msg = FString::Printf(
				TEXT("[Nova] 경고: 현재 레벨이 GameMode를 '%s'로 오버라이드 중입니다. Project Settings의 NovaGameMode/컨트롤러 설정이 적용되지 않을 수 있습니다. (World Settings -> GameMode Override 확인)"),
				*GMName
			);

			UE_LOG(LogTemp, Display, TEXT("NOVA %s"), *Msg);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Yellow, Msg);
			}
		}
	}

	static void ShowPIEDebugPopup(const bool bIsSimulating)
	{
		(void)bIsSimulating;

		if (!GEditor)
		{
			return;
		}

		UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : nullptr;
		if (!World)
		{
			return;
		}

		AGameModeBase* GM = World->GetAuthGameMode();
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;

		const FString WorldName = World->GetName();
		const FString GMName = GM ? GM->GetClass()->GetName() : TEXT("None");
		const FString PCName = PC ? PC->GetClass()->GetName() : TEXT("None");
		const FString PawnName = Pawn ? Pawn->GetClass()->GetName() : TEXT("None");

		const FString OverrideGMName =
			(World->GetWorldSettings() && World->GetWorldSettings()->DefaultGameMode)
				? World->GetWorldSettings()->DefaultGameMode->GetName()
				: TEXT("None");

		const FString Msg = FString::Printf(
			TEXT("NOVA PIE 확인\n\nWorld: %s\nWorldSettings Override GM: %s\nAuth GameMode: %s\nPlayerController: %s\nPawn: %s\n\n(여기서 PlayerController가 NovaClickMovePlayerController가 아니면, 시점/클릭무브 코드는 안 탑니다.)"),
			*WorldName,
			*OverrideGMName,
			*GMName,
			*PCName,
			*PawnName
		);

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));
	}
#endif
}

class FNovaUprojectModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

#if WITH_EDITOR
		using namespace NovaUprojectEditorWarnings;
		if (!BeginPIEHandle.IsValid())
		{
			BeginPIEHandle = FEditorDelegates::BeginPIE.AddStatic(&WarnIfGameModeOverridden);
		}

		if (!PostPIEStartedHandle.IsValid())
		{
			PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddStatic(&ShowPIEDebugPopup);
		}
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		using namespace NovaUprojectEditorWarnings;
		if (BeginPIEHandle.IsValid())
		{
			FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
			BeginPIEHandle.Reset();
		}

		if (PostPIEStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
			PostPIEStartedHandle.Reset();
		}
#endif

		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FNovaUprojectModule, NovaUproject, "NovaUproject");
