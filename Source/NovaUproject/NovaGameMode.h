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

	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	float DungeonNavAgentMaxStepHeight = 60.f;
};

