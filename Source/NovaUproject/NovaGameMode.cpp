#include "NovaGameMode.h"

#include "UObject/ConstructorHelpers.h"
#include "NovaClickMovePlayerController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
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
	ApplyBossRoomBrightnessIfNeeded();
}

void ANovaGameMode::ApplyBossRoomBrightnessIfNeeded()
{
	UWorld* World = GetWorld();
	if (!World || BossRoomMapNameToken.IsNone())
	{
		return;
	}

	const FString MapName = World->GetMapName();
	if (!MapName.Contains(BossRoomMapNameToken.ToString(), ESearchCase::IgnoreCase))
	{
		return;
	}

	int32 BoostedLightCount = 0;

	auto BoostLightComponent = [&](ULightComponent* LightComponent)
	{
		if (!LightComponent)
		{
			return;
		}

		LightComponent->SetIntensity(LightComponent->Intensity * BossRoomLightBrightnessMultiplier);
		++BoostedLightCount;
	};

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		BoostLightComponent(It->GetLightComponent());
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* SkyLightComponent = It->GetLightComponent())
		{
			SkyLightComponent->SetIntensity(SkyLightComponent->Intensity * BossRoomLightBrightnessMultiplier);
			SkyLightComponent->RecaptureSky();
			++BoostedLightCount;
		}
	}

	for (TActorIterator<APointLight> It(World); It; ++It)
	{
		BoostLightComponent(It->GetLightComponent());
	}

	for (TActorIterator<ASpotLight> It(World); It; ++It)
	{
		BoostLightComponent(It->GetLightComponent());
	}

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		APostProcessVolume* PostProcessVolume = *It;
		if (!PostProcessVolume)
		{
			continue;
		}

		FPostProcessSettings& Settings = PostProcessVolume->Settings;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias += BossRoomExposureBiasBoost;
		PostProcessVolume->bEnabled = true;
	}

	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* FogComponent = It->GetComponent())
		{
			FogComponent->SetFogDensity(FogComponent->FogDensity * BossRoomFogDensityMultiplier);
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Nova: Boss room brightness boost applied (%s, lights=%d, x%.2f)"),
		*MapName,
		BoostedLightCount,
		BossRoomLightBrightnessMultiplier);
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
