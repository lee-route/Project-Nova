#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NovaClickMovePlayerController.generated.h"

UCLASS()
class NOVAUPROJECT_API ANovaClickMovePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANovaClickMovePlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void OnLeftClickPressed();
	void OnLeftClickReleased();
	void UpdateDestinationUnderCursor(bool bPrintDebug);

	bool bHasDestination = false;
	FVector Destination = FVector::ZeroVector;

	bool bIsHoldingMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float AcceptanceRadius = 50.0f;
};

