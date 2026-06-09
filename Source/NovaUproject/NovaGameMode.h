#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NovaGameMode.generated.h"

UCLASS()
class NOVAUPROJECT_API ANovaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANovaGameMode();

protected:
	virtual void StartPlay() override;
	void ConfigureDungeonNavMesh();
	void ApplyBossRoomBrightnessIfNeeded();

	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	float DungeonNavAgentMaxStepHeight = 75.f;

	/** 맵 이름에 이 문자열이 포함되면 조명을 살짝 밝힙니다 (예: map_dungeon_level_5_bossroom) */
	UPROPERTY(EditDefaultsOnly, Category = "Lighting|BossRoom")
	FName BossRoomMapNameToken = TEXT("bossroom");

	UPROPERTY(EditDefaultsOnly, Category = "Lighting|BossRoom", meta = (ClampMin = "1.0"))
	float BossRoomLightBrightnessMultiplier = 1.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Lighting|BossRoom")
	float BossRoomExposureBiasBoost = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Lighting|BossRoom", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float BossRoomFogDensityMultiplier = 0.75f;
};

