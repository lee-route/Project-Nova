#include "NovaGameMode.h"

#include "UObject/ConstructorHelpers.h"
#include "NovaClickMovePlayerController.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

ANovaGameMode::ANovaGameMode()
{
	// 공유 프로젝트(던전): 1인칭 Terra 캐릭터 + 대화/NPC 연동 PC
	static ConstructorHelpers::FClassFinder<APawn> FirstPersonPawnBP(
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter")
	);
	if (FirstPersonPawnBP.Succeeded())
	{
		DefaultPawnClass = FirstPersonPawnBP.Class;
	}

	// 던전: BP_NovaPlayerController (NovaClickMovePlayerController 자식, 음성·대화·조작 통합)
	// bp_npc Cast는 CoreRedirects로 BP_FirstPersonPlayerController → BP_NovaPlayerController 리맵
	static ConstructorHelpers::FClassFinder<APlayerController> NovaPCBp(
		TEXT("/Game/ThirdPerson/Blueprints/BP_NovaPlayerController")
	);
	if (NovaPCBp.Succeeded())
	{
		PlayerControllerClass = NovaPCBp.Class;
	}
	else
	{
		PlayerControllerClass = ANovaClickMovePlayerController::StaticClass();
	}
}

void ANovaGameMode::StartPlay()
{
	Super::StartPlay();
	ConfigureDungeonNavMesh();
}

void ANovaGameMode::ConfigureDungeonNavMesh()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bool bNeedsRebuild = false;
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		ARecastNavMesh* NavMesh = *It;
		if (!NavMesh)
		{
			continue;
		}

		for (uint8 ResolutionIndex = 0; ResolutionIndex < static_cast<uint8>(ENavigationDataResolution::MAX); ++ResolutionIndex)
		{
			const ENavigationDataResolution Resolution = static_cast<ENavigationDataResolution>(ResolutionIndex);
			if (NavMesh->GetAgentMaxStepHeight(Resolution) < DungeonNavAgentMaxStepHeight)
			{
				NavMesh->SetAgentMaxStepHeight(Resolution, DungeonNavAgentMaxStepHeight);
				bNeedsRebuild = true;
			}
		}
	}

	if (!bNeedsRebuild)
	{
		return;
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		UE_LOG(LogTemp, Display, TEXT("Nova: Rebuilding NavMesh (AgentMaxStepHeight=%.0f) for stairs"), DungeonNavAgentMaxStepHeight);
		NavSys->Build();
	}
}
