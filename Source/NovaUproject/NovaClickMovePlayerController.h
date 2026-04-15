#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NovaClickMovePlayerController.generated.h"

class UInputMappingContext;
class UFXSystemAsset;

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

	// Exposed so Blueprint child (Class Defaults) can set VFX easily.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext = nullptr;

	// Optional VFX spawned when setting a click-move destination.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ClickMove|VFX", meta = (AllowPrivateAccess = "true"))
	UFXSystemAsset* ClickMoveIndicatorFx = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ClickMove|VFX", meta = (AllowPrivateAccess = "true"))
	float ClickMoveIndicatorScale = 1.0f;

private:
	void OnLeftClickPressed();
	void OnLeftClickReleased();
	void UpdateDestinationUnderCursor(bool bPrintDebug);

	void OnVPressed();
	void MoveForward(float Value);
	void MoveRight(float Value);

	void ToggleCameraMode();
	void ApplyTopDownCamera();
	void ApplyThirdPersonCamera();

	void EnsureDefaultMappingContext();
	void SpawnClickMoveIndicator(const FVector& WorldLocation);

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

	UPROPERTY(VisibleInstanceOnly, Category = "Camera")
	bool bIsTopDownCamera = true;
};

