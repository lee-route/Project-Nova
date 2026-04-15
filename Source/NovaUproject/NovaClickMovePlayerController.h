#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NovaClickMovePlayerController.generated.h"

class UFXSystemAsset;
class UUserWidget;

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
	void OnDashPressed();
	void MoveForward(float Value);
	void MoveRight(float Value);
	void ApplyTopDownCamera();
	void SpawnClickMoveIndicator(const FVector& WorldLocation);

	bool bHasDestination = false;
	FVector Destination = FVector::ZeroVector;

	bool bIsHoldingMove = false;

	UPROPERTY(EditDefaultsOnly, Category = "ClickMove")
	float AcceptanceRadius = 50.0f;

	// If true, show cursor only while LMB is held.
	UPROPERTY(EditDefaultsOnly, Category = "Cursor")
	bool bShowCursorWhileHoldingMove = true;

	// Optional: replace OS cursor with a UMG cursor widget (e.g. arrow image).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Cursor", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> CursorWidgetClass;

	UPROPERTY(VisibleInstanceOnly, Category = "Control")
	ENovaControlMode ControlMode = ENovaControlMode::ClickMove;

	UPROPERTY(VisibleInstanceOnly, Category = "Camera")
	bool bIsTopDownCamera = true;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashStrength = 1600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashUpwardStrength = 0.0f;
};

