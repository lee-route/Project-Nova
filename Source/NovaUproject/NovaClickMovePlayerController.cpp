// Fill out your copyright notice in the Description page of Project Settings.

#include "NovaClickMovePlayerController.h"

#include "InputCoreTypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"

ANovaClickMovePlayerController::ANovaClickMovePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	DefaultMouseCursor = EMouseCursor::Default;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC_Default(
		TEXT("/Game/Input/IMC_Default.IMC_Default")
	);
	if (IMC_Default.Succeeded())
	{
		DefaultMappingContext = IMC_Default.Object;
	}
}

void ANovaClickMovePlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Display, TEXT("NOVA PC BeginPlay: %s Pawn=%s World=%s"),
		*GetClass()->GetName(),
		GetPawn() ? *GetPawn()->GetClass()->GetName() : TEXT("None"),
		GetWorld() ? *GetWorld()->GetName() : TEXT("None"));

	// Ensure mouse clicks are routed to the game.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Game+UI so cursor remains visible and consistent.
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	// Enhanced Input: ensure IMC_Default is applied (Dash, Move, etc).
	EnsureDefaultMappingContext();

	// PIE 시작 시점은 기본으로 디아블로(탑다운) 시점.
	bIsTopDownCamera = true;
	ApplyTopDownCamera();

	if (GEngine)
	{
		const FString PawnName = GetPawn() ? GetPawn()->GetClass()->GetName() : TEXT("None");
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Cyan,
			FString::Printf(TEXT("Nova PC BeginPlay. Pawn=%s (V: camera, Shift+V: control)"), *PawnName)
		);
	}
}

void ANovaClickMovePlayerController::EnsureDefaultMappingContext()
{
	if (!DefaultMappingContext)
	{
		UE_LOG(LogTemp, Display, TEXT("NOVA IMC_Default not found. Dash may not work."));
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
		UE_LOG(LogTemp, Display, TEXT("NOVA IMC_Default applied."));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("NOVA EnhancedInput subsystem missing. Dash may not work."));
	}
}

void ANovaClickMovePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Display, TEXT("NOVA PC SetupInputComponent: %s InputComponent=%s"),
		*GetClass()->GetName(),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	// No project input mappings required.
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ANovaClickMovePlayerController::OnLeftClickPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ANovaClickMovePlayerController::OnLeftClickReleased);

	// Shift + V: toggle control mode
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ANovaClickMovePlayerController::OnVPressed);

	// Legacy axis mappings (Project Settings -> Input)
	InputComponent->BindAxis(TEXT("MoveForward"), this, &ANovaClickMovePlayerController::MoveForward);
	InputComponent->BindAxis(TEXT("MoveRight"), this, &ANovaClickMovePlayerController::MoveRight);
}

void ANovaClickMovePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// While holding LMB, keep updating the destination under cursor.
	// When released, we keep moving to the last destination until reached.
	if (ControlMode == ENovaControlMode::ClickMove && bIsHoldingMove)
	{
		UpdateDestinationUnderCursor(/*bPrintDebug*/ false);
	}

	if (!bHasDestination)
	{
		return;
	}

	if (ControlMode != ENovaControlMode::ClickMove)
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

void ANovaClickMovePlayerController::SetControlMode(ENovaControlMode NewMode)
{
	if (ControlMode == NewMode)
	{
		return;
	}

	ControlMode = NewMode;

	// When switching to WASD, cancel any click-move in progress.
	if (ControlMode == ENovaControlMode::WASD)
	{
		bIsHoldingMove = false;
		bHasDestination = false;
	}
}

void ANovaClickMovePlayerController::OnLeftClickPressed()
{
	if (ControlMode != ENovaControlMode::ClickMove)
	{
		return;
	}

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

	// Spawn an optional click-move indicator on fresh clicks (and when debug is requested).
	if (bPrintDebug)
	{
		SpawnClickMoveIndicator(Destination);
	}

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

void ANovaClickMovePlayerController::SpawnClickMoveIndicator(const FVector& WorldLocation)
{
	if (!ClickMoveIndicatorFx)
	{
		return;
	}

	if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(ClickMoveIndicatorFx))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			Niagara,
			WorldLocation,
			FRotator::ZeroRotator,
			FVector(ClickMoveIndicatorScale),
			/*bAutoDestroy*/ true,
			/*bAutoActivate*/ true,
			/*PoolingMethod*/ ENCPoolMethod::AutoRelease,
			/*bPreCullCheck*/ true
		);
		return;
	}

	if (UParticleSystem* Particle = Cast<UParticleSystem>(ClickMoveIndicatorFx))
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			Particle,
			FTransform(FRotator::ZeroRotator, WorldLocation, FVector(ClickMoveIndicatorScale)),
			/*bAutoDestroy*/ true
		);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("NOVA ClickMoveIndicatorFx has unsupported type: %s"), *ClickMoveIndicatorFx->GetClass()->GetName());
}

void ANovaClickMovePlayerController::OnVPressed()
{
	UE_LOG(LogTemp, Display, TEXT("NOVA V Pressed. Shift=%d Pawn=%s"),
		(IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)) ? 1 : 0,
		GetPawn() ? *GetPawn()->GetClass()->GetName() : TEXT("None"));

	const bool bShiftDown = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if (!bShiftDown)
	{
		// V alone: toggle camera mode (top-down <-> 3rd person) on the currently possessed pawn,
		// even if it's a blueprint template character.
		ToggleCameraMode();

		return;
	}

	const ENovaControlMode NewMode =
		(ControlMode == ENovaControlMode::ClickMove) ? ENovaControlMode::WASD : ENovaControlMode::ClickMove;

	// Requested style: call SetControlMode with a different argument each press.
	SetControlMode(NewMode);
}

void ANovaClickMovePlayerController::ToggleCameraMode()
{
	APawn* P = GetPawn();
	if (!P)
	{
		UE_LOG(LogTemp, Display, TEXT("NOVA ToggleCameraMode aborted: no pawn"));
		return;
	}

	bIsTopDownCamera = !bIsTopDownCamera;
	UE_LOG(LogTemp, Display, TEXT("NOVA ToggleCameraMode -> %s"),
		bIsTopDownCamera ? TEXT("TopDown") : TEXT("ThirdPerson"));

	if (bIsTopDownCamera)
	{
		ApplyTopDownCamera();
	}
	else
	{
		ApplyThirdPersonCamera();
	}
}

static USpringArmComponent* FindSpringArmOnPawn(APawn* P)
{
	if (!P)
	{
		return nullptr;
	}

	// Works for both C++ and Blueprint-added components.
	if (USpringArmComponent* Arm = P->FindComponentByClass<USpringArmComponent>())
	{
		return Arm;
	}

	return nullptr;
}

static UCameraComponent* FindCameraOnPawn(APawn* P)
{
	if (!P)
	{
		return nullptr;
	}

	if (UCameraComponent* Cam = P->FindComponentByClass<UCameraComponent>())
	{
		return Cam;
	}

	return nullptr;
}

void ANovaClickMovePlayerController::ApplyTopDownCamera()
{
	APawn* P = GetPawn();
	USpringArmComponent* Arm = FindSpringArmOnPawn(P);
	UCameraComponent* Cam = FindCameraOnPawn(P);
	if (!Arm || !Cam)
	{
		UE_LOG(LogTemp, Display, TEXT("NOVA ApplyTopDownCamera failed: Arm=%s Cam=%s Pawn=%s"),
			Arm ? *Arm->GetName() : TEXT("None"),
			Cam ? *Cam->GetName() : TEXT("None"),
			P ? *P->GetClass()->GetName() : TEXT("None"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No SpringArm/Camera found on Pawn"));
		}
		return;
	}

	Arm->TargetArmLength = 1000.0f;
	Arm->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));
	Arm->bUsePawnControlRotation = false;
	Arm->bInheritPitch = false;
	Arm->bInheritRoll = false;
	Arm->bInheritYaw = false;
	Arm->bDoCollisionTest = false;

	Cam->bUsePawnControlRotation = false;

	// For top-down, lock control yaw so WASD doesn't feel "diagonal" due to leftover controller rotation.
	SetControlRotation(FRotator(0.f, 0.f, 0.f));
}

void ANovaClickMovePlayerController::ApplyThirdPersonCamera()
{
	APawn* P = GetPawn();
	USpringArmComponent* Arm = FindSpringArmOnPawn(P);
	UCameraComponent* Cam = FindCameraOnPawn(P);
	if (!Arm || !Cam)
	{
		UE_LOG(LogTemp, Display, TEXT("NOVA ApplyThirdPersonCamera failed: Arm=%s Cam=%s Pawn=%s"),
			Arm ? *Arm->GetName() : TEXT("None"),
			Cam ? *Cam->GetName() : TEXT("None"),
			P ? *P->GetClass()->GetName() : TEXT("None"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No SpringArm/Camera found on Pawn"));
		}
		return;
	}

	Arm->TargetArmLength = 400.0f;
	Arm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
	Arm->bUsePawnControlRotation = true;
	Arm->bInheritPitch = true;
	Arm->bInheritRoll = true;
	Arm->bInheritYaw = true;
	Arm->bDoCollisionTest = true;

	Cam->bUsePawnControlRotation = false;
}

void ANovaClickMovePlayerController::MoveForward(float Value)
{
	if (ControlMode != ENovaControlMode::WASD || FMath::IsNearlyZero(Value))
	{
		return;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	// In top-down mode, use world axes to keep movement consistent.
	if (bIsTopDownCamera)
	{
		P->AddMovementInput(FVector::ForwardVector, Value);
		return;
	}

	const FRotator ControlRot = GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	P->AddMovementInput(Forward, Value);
}

void ANovaClickMovePlayerController::MoveRight(float Value)
{
	if (ControlMode != ENovaControlMode::WASD || FMath::IsNearlyZero(Value))
	{
		return;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	if (bIsTopDownCamera)
	{
		P->AddMovementInput(FVector::RightVector, Value);
		return;
	}

	const FRotator ControlRot = GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	P->AddMovementInput(Right, Value);
}

