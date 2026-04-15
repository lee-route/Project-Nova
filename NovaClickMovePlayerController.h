#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NovaClickMovePlayerController.generated.h"

UENUM(BlueprintType)
enum class ENovaControlMode : uint8
{
	ClickMove UMETA(DisplayName = "Click Move"),
	WASD UMETA(DisplayName = "WASD")
};

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

	void SetControlMode(ENovaControlMode NewMode);

private:
	void OnLeftClickPressed();
	void OnLeftClickReleased();
	void UpdateDestinationUnderCursor(bool bPrintDebug);

	void OnVPressed();
	void MoveForward(float Value);
	void MoveRight(float Value);

	bool bHasDestination = false;
	FVector Destination = FVector::ZeroVector;

	bool bIsHoldingMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float AcceptanceRadius = 50.0f;

	// If true, show cursor only while LMB is held.
	UPROPERTY(EditDefaultsOnly, Category = "Cursor")
	bool bShowCursorWhileHoldingMove = true;

	UPROPERTY(VisibleInstanceOnly, Category = "Control")
	ENovaControlMode ControlMode = ENovaControlMode::ClickMove;
};

