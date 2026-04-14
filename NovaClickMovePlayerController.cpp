// Fill out your copyright notice in the Description page of Project Settings.

#include "NovaClickMovePlayerController.h"

#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

ANovaClickMovePlayerController::ANovaClickMovePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	DefaultMouseCursor = EMouseCursor::Default;
}

void ANovaClickMovePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Ensure mouse clicks are routed to the game.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Game+UI input mode tends to be the most reliable for mouse cursor projects.
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

void ANovaClickMovePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// No project input mappings required.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ANovaClickMovePlayerController::OnLeftClickPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ANovaClickMovePlayerController::OnLeftClickReleased);
}

void ANovaClickMovePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// While holding LMB, keep updating the destination under cursor.
	// When released, we keep moving to the last destination until reached.
	if (bIsHoldingMove)
	{
		UpdateDestinationUnderCursor(/*bPrintDebug*/ false);
	}

	if (!bHasDestination)
	{
		return;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	const FVector Current = P->GetActorLocation();
	FVector To = Destination - Current;
	To.Z = 0.f;

	const float DistSq = To.SizeSquared();
	if (DistSq <= FMath::Square(AcceptanceRadius))
	{
		bHasDestination = false;
		return;
	}

	const FVector Dir = To.GetSafeNormal();
	P->AddMovementInput(Dir, 1.0f);
}

void ANovaClickMovePlayerController::OnLeftClickPressed()
{
	bIsHoldingMove = true;
	UpdateDestinationUnderCursor(/*bPrintDebug*/ true);
}

void ANovaClickMovePlayerController::OnLeftClickReleased()
{
	bIsHoldingMove = false;
	// Intentionally keep bHasDestination as-is so we continue to the last point.
}

void ANovaClickMovePlayerController::UpdateDestinationUnderCursor(bool bPrintDebug)
{
	FHitResult Hit;
	bool bHit = GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Hit);
	if (!bHit)
	{
		// Some meshes don't block Visibility; Camera is a common alternative.
		bHit = GetHitResultUnderCursor(ECC_Camera, /*bTraceComplex*/ false, Hit);
	}

	if (!bHit)
	{
		if (bPrintDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Red, TEXT("No hit under cursor"));
		}
		return;
	}

	Destination = Hit.Location;
	bHasDestination = true;

	if (bPrintDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Green,
			FString::Printf(TEXT("Move to: X=%.0f Y=%.0f Z=%.0f"), Destination.X, Destination.Y, Destination.Z)
		);
	}
}

