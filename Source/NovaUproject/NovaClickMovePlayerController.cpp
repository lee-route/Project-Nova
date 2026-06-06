// Fill out your copyright notice in the Description page of Project Settings.

#include "NovaClickMovePlayerController.h"

#include "NovaCombatVoiceGateComponent.h"
#include "NovaVoiceCaptureComponent.h"
#include "InputCoreTypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Blueprint/UserWidget.h"

ANovaClickMovePlayerController::ANovaClickMovePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	DefaultMouseCursor = EMouseCursor::Default;

	VoiceCaptureComponent = CreateDefaultSubobject<UNovaVoiceCaptureComponent>(TEXT("VoiceCaptureComponent"));
	CombatVoiceGateComponent = CreateDefaultSubobject<UNovaCombatVoiceGateComponent>(TEXT("CombatVoiceGateComponent"));

	if (VoiceCaptureComponent)
	{
		VoiceCaptureComponent->OnVoiceCommandRecognized.AddDynamic(this, &ANovaClickMovePlayerController::OnVoiceCommandRecognized);
	}

	if (CombatVoiceGateComponent)
	{
		CombatVoiceGateComponent->OnCounterSucceeded.AddDynamic(this, &ANovaClickMovePlayerController::OnCounterSucceeded);
	}
}

namespace NovaVoiceDebug
{
	static FString CommandToDisplayName(ENovaVoiceCommand Command)
	{
		switch (Command)
		{
		case ENovaVoiceCommand::Bow: return TEXT("활");
		case ENovaVoiceCommand::Shield: return TEXT("방패");
		case ENovaVoiceCommand::Scythe: return TEXT("낫");
		case ENovaVoiceCommand::Hammer: return TEXT("검");
		case ENovaVoiceCommand::Help: return TEXT("도와줘");
		case ENovaVoiceCommand::Cancel: return TEXT("취소");
		default: return TEXT("None");
		}
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

	// Optional: replace OS cursor image with a UMG widget cursor.
	if (CursorWidgetClass)
	{
		if (UUserWidget* W = CreateWidget<UUserWidget>(this, CursorWidgetClass))
		{
			SetMouseCursorWidget(EMouseCursor::Default, W);
		}
	}

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
			FString::Printf(TEXT("Nova PC BeginPlay. Pawn=%s (V: control, 1-4: weapon, F5-F8: counter test)"), *PawnName)
		);

		if (VoiceCaptureComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("NOVA VoiceCaptureComponent exists. Before StartListening: %s"),
				VoiceCaptureComponent->IsListening() ? TEXT("Listening") : TEXT("Not Listening"));
		
			const bool bStarted = VoiceCaptureComponent->StartListening();
		
			UE_LOG(LogTemp, Warning, TEXT("NOVA VoiceCaptureComponent StartListening result: %s / Current: %s"),
				bStarted ? TEXT("Success") : TEXT("Failed"),
				VoiceCaptureComponent->IsListening() ? TEXT("Listening") : TEXT("Not Listening"));
		
			const FString VoiceStatus = VoiceCaptureComponent->IsListening()
				? TEXT("Voice: listening")
				: TEXT("Voice: not listening - check ini or microphone");
		
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, VoiceStatus);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("NOVA VoiceCaptureComponent is NULL"));
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("VoiceCaptureComponent NULL"));
		}
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

	// Space: dash (jump removed)
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ANovaClickMovePlayerController::OnDashPressed);

	// Legacy axis mappings (Project Settings -> Input)
	InputComponent->BindAxis(TEXT("MoveForward"), this, &ANovaClickMovePlayerController::MoveForward);
	InputComponent->BindAxis(TEXT("MoveRight"), this, &ANovaClickMovePlayerController::MoveRight);

	// Keyboard fallback for weapon switching.
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey4);

	// Debug keys to simulate boss counter windows from the report.
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF5);
	InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF6);
	InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF7);
	InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF8);
	// Skill keys for current weapon.
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ANovaClickMovePlayerController::OnSkillQ);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ANovaClickMovePlayerController::OnSkillW);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ANovaClickMovePlayerController::OnSkillE);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ANovaClickMovePlayerController::OnSkillR);
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

	if (VoiceCaptureComponent && GEngine)
	{
		static float DebugAccumulator = 0.0f;
		DebugAccumulator += DeltaTime;
		if (DebugAccumulator >= 2.0f)
		{
			DebugAccumulator = 0.0f;
			GEngine->AddOnScreenDebugMessage(
				9001,
				2.1f,
				FColor::White,
				VoiceCaptureComponent->GetDebugSummary()
			);
		}
	}
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
	UE_LOG(LogTemp, Display, TEXT("NOVA V Pressed. Pawn=%s"),
		GetPawn() ? *GetPawn()->GetClass()->GetName() : TEXT("None"));

	// V: toggle control mode (ClickMove <-> WASD)
	const ENovaControlMode NewMode =
		(ControlMode == ENovaControlMode::ClickMove) ? ENovaControlMode::WASD : ENovaControlMode::ClickMove;

	SetControlMode(NewMode);
}

void ANovaClickMovePlayerController::OnDashPressed()
{
	ACharacter* C = Cast<ACharacter>(GetPawn());
	if (!C)
	{
		return;
	}

	FVector Dir = FVector::ZeroVector;

	if (ControlMode == ENovaControlMode::ClickMove)
	{
		if (bHasDestination)
		{
			Dir = (Destination - C->GetActorLocation());
			Dir.Z = 0.f;
			Dir = Dir.GetSafeNormal();
		}
		else
		{
			FHitResult Hit;
			if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
			{
				Dir = (Hit.Location - C->GetActorLocation());
				Dir.Z = 0.f;
				Dir = Dir.GetSafeNormal();
			}
		}
	}

	if (Dir.IsNearlyZero())
	{
		Dir = C->GetLastMovementInputVector();
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
	}

	if (Dir.IsNearlyZero())
	{
		Dir = C->GetActorForwardVector();
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
	}

	const FVector LaunchVel = Dir * DashStrength + FVector(0.f, 0.f, DashUpwardStrength);
	C->LaunchCharacter(LaunchVel, /*bXYOverride*/ true, /*bZOverride*/ true);
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

bool ANovaClickMovePlayerController::SwitchSecondaryWeapon(ENovaVoiceCommand WeaponCommand)
{
	if (WeaponCommand != ENovaVoiceCommand::Bow
		&& WeaponCommand != ENovaVoiceCommand::Shield
		&& WeaponCommand != ENovaVoiceCommand::Scythe
		&& WeaponCommand != ENovaVoiceCommand::Hammer)
	{
		return false;
	}

	EquippedSecondaryWeapon = WeaponCommand;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Green,
			FString::Printf(TEXT("Weapon switched: %s"), *NovaVoiceDebug::CommandToDisplayName(WeaponCommand))
		);
	}

	return true;
}

void ANovaClickMovePlayerController::OpenBossCounterWindow(ENovaBossCounterType CounterType)
{
	if (CombatVoiceGateComponent)
	{
		CombatVoiceGateComponent->OpenCounterWindow(CounterType);
	}
}

void ANovaClickMovePlayerController::OnVoiceCommandRecognized(const FNovaVoiceCommandResult& CommandResult)
{
	FString RejectReason;
	if (CombatVoiceGateComponent && !CombatVoiceGateComponent->TryAcceptVoiceCommand(CommandResult, RejectReason))
	{
		if (GEngine && !RejectReason.IsEmpty())
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, RejectReason);
		}
		return;
	}

	switch (CommandResult.Command)
	{
	case ENovaVoiceCommand::Bow:
	case ENovaVoiceCommand::Shield:
	case ENovaVoiceCommand::Scythe:
	case ENovaVoiceCommand::Hammer:
		HandleWeaponSwitchInput(CommandResult.Command);
		break;
	case ENovaVoiceCommand::Help:
		RequestCompanionHelp();
		break;
	case ENovaVoiceCommand::Cancel:
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Silver, TEXT("Voice cancel received"));
		}
		break;
	default:
		break;
	}
}

void ANovaClickMovePlayerController::HandleWeaponSwitchInput(ENovaVoiceCommand WeaponCommand)
{
	SwitchSecondaryWeapon(WeaponCommand);
}

void ANovaClickMovePlayerController::RequestCompanionHelp()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("Companion help requested (voice)"));
	}
}

void ANovaClickMovePlayerController::OnCounterSucceeded(ENovaBossCounterType CounterType, ENovaVoiceCommand Command)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Emerald,
			FString::Printf(
				TEXT("Boss counter success: pattern=%d weapon=%s"),
				static_cast<int32>(CounterType),
				*NovaVoiceDebug::CommandToDisplayName(Command)
			)
		);
	}
}

void ANovaClickMovePlayerController::OnWeaponKey1() { HandleWeaponSwitchInput(ENovaVoiceCommand::Shield); }
void ANovaClickMovePlayerController::OnWeaponKey2() { HandleWeaponSwitchInput(ENovaVoiceCommand::Scythe); }
void ANovaClickMovePlayerController::OnWeaponKey3() { HandleWeaponSwitchInput(ENovaVoiceCommand::Bow); }
void ANovaClickMovePlayerController::OnWeaponKey4() { HandleWeaponSwitchInput(ENovaVoiceCommand::Hammer); }

void ANovaClickMovePlayerController::OnDebugCounterKeyF5() { OpenBossCounterWindow(ENovaBossCounterType::LaserShield); }
void ANovaClickMovePlayerController::OnDebugCounterKeyF6() { OpenBossCounterWindow(ENovaBossCounterType::SpaceScythe); }
void ANovaClickMovePlayerController::OnDebugCounterKeyF7() { OpenBossCounterWindow(ENovaBossCounterType::SummonBow); }
void ANovaClickMovePlayerController::OnDebugCounterKeyF8() { OpenBossCounterWindow(ENovaBossCounterType::DebrisHammer); }


void ANovaClickMovePlayerController::OnSkillQ()
{
	BP_UseSkillQ(EquippedSecondaryWeapon);
}

void ANovaClickMovePlayerController::OnSkillW()
{
	BP_UseSkillW(EquippedSecondaryWeapon);
}

void ANovaClickMovePlayerController::OnSkillE()
{
	BP_UseSkillE(EquippedSecondaryWeapon);
}

void ANovaClickMovePlayerController::OnSkillR()
{
	BP_UseSkillR(EquippedSecondaryWeapon);
}