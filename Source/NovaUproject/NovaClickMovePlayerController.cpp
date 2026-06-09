// Fill out your copyright notice in the Description page of Project Settings.

#include "NovaClickMovePlayerController.h"

#include "NovaCombatVoiceGateComponent.h"
#include "NovaSecondaryWeaponVisualComponent.h"
#include "NovaVoiceCaptureComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectGlobals.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

ANovaClickMovePlayerController::ANovaClickMovePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	DefaultMouseCursor = EMouseCursor::Default;

	VoiceCaptureComponent = CreateDefaultSubobject<UNovaVoiceCaptureComponent>(TEXT("VoiceCaptureComponent"));
	CombatVoiceGateComponent = CreateDefaultSubobject<UNovaCombatVoiceGateComponent>(TEXT("CombatVoiceGateComponent"));

	static ConstructorHelpers::FClassFinder<UUserWidget> DialogueWidgetBP(
		TEXT("/Game/Fantastic_Dungeon_Pack/maps/WBP_Dialogue")
	);
	if (DialogueWidgetBP.Succeeded())
	{
		DialogueWidgetClass = DialogueWidgetBP.Class;
	}

	if (VoiceCaptureComponent)
	{
		VoiceCaptureComponent->OnVoiceCommandRecognized.AddDynamic(this, &ANovaClickMovePlayerController::OnVoiceCommandRecognized);
	}

	if (CombatVoiceGateComponent)
	{
		CombatVoiceGateComponent->OnCounterSucceeded.AddDynamic(this, &ANovaClickMovePlayerController::OnCounterSucceeded);
	}
}

namespace NovaWeaponDisplay
{
	static FString CommandToDisplayName(ENovaVoiceCommand Command)
	{
		switch (Command)
		{
		case ENovaVoiceCommand::Hammer: return TEXT("Sword (칼)");
		case ENovaVoiceCommand::Bow: return TEXT("Bow (활)");
		case ENovaVoiceCommand::Scythe: return TEXT("Spear (창)");
		case ENovaVoiceCommand::Shield: return TEXT("Shield (방패)");
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

	// 통합 조작: 우클릭 이동 — 커서 필요
	ApplyGameplayInputMode();

	// Optional: replace OS cursor image with a UMG widget cursor.
	if (CursorWidgetClass)
	{
		if (UUserWidget* W = CreateWidget<UUserWidget>(this, CursorWidgetClass))
		{
			SetMouseCursorWidget(EMouseCursor::Default, W);
		}
	}

	bIsTopDownCamera = !IsFirstPersonDungeonPawn();
	ApplyFixedOrbitCamera();

	if (GEngine)
	{
		const FString PawnName = GetPawn() ? GetPawn()->GetClass()->GetName() : TEXT("None");
		GEngine->AddOnScreenDebugMessage(
			-1,
			10.0f,
			FColor::Cyan,
			FString::Printf(
				TEXT("Nova 조작 | RMB이동 LMB대시 Space점프 A/D시점 QWER F대화 G종료 | Pawn=%s"),
				*PawnName
			)
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

void ANovaClickMovePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	bIsTopDownCamera = !IsFirstPersonDungeonPawn();
	if (InPawn)
	{
		CameraYawDegrees = FMath::UnwindDegrees(InPawn->GetActorRotation().Yaw - CharacterFacingYawOffset);
	}
	else
	{
		CameraYawDegrees = GetControlRotation().Yaw;
	}

	// Pawn Enhanced Input(WASD/F/Space 등)과 C++ 조작 충돌 방지 — 이동은 PC가 AddMovementInput으로 처리
	if (InPawn)
	{
		InPawn->DisableInput(this);
		ApplyMovementRotationSettings(InPawn);
	}

	FRotator ControlRot = GetControlRotation();
	ControlRot.Yaw = CameraYawDegrees;
	SetControlRotation(ControlRot);

	ApplyFixedOrbitCamera();
	ApplyWeaponVisualToPawn(EquippedSecondaryWeapon);
	ApplyGameplayInputMode();
}

void ANovaClickMovePlayerController::OnUnPossess()
{
	if (APawn* P = GetPawn())
	{
		P->EnableInput(this);
	}

	Super::OnUnPossess();
}

void ANovaClickMovePlayerController::ApplyGameplayInputMode()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// 마우스 시점 입력 차단 — A/D만 TurnCamera()에서 SetControlRotation으로 처리
	SetIgnoreLookInput(true);

	if (IsDialogueActive())
	{
		FInputModeGameAndUI Mode;
		if (ActiveDialogueWidget)
		{
			Mode.SetWidgetToFocus(ActiveDialogueWidget->TakeWidget());
		}
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		return;
	}

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ANovaClickMovePlayerController::ApplyMovementRotationSettings(APawn* InPawn)
{
	ACharacter* ControlledCharacter = Cast<ACharacter>(InPawn);
	if (!ControlledCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* Move = ControlledCharacter->GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
		Move->MaxStepHeight = MaxStepHeight;
		Move->SetWalkableFloorAngle(MaxWalkableFloorAngle);
		Move->bCanWalkOffLedges = true;
	}

	ControlledCharacter->bUseControllerRotationYaw = false;
	ControlledCharacter->bUseControllerRotationPitch = false;
	ControlledCharacter->bUseControllerRotationRoll = false;
}

void ANovaClickMovePlayerController::ConsumeLookAxis(float Value)
{
	// MouseX/MouseY/Turn/LookUp 흡수 — 시점은 A/D 전용
}

void ANovaClickMovePlayerController::TickDialogueInput()
{
	const bool bGDown = IsInputKeyDown(EKeys::G);
	if (bGDown && !bPrevGKeyDown)
	{
		EndDialogueMode();
	}
	bPrevGKeyDown = bGDown;
}

bool ANovaClickMovePlayerController::IsDialogueActive() const
{
	return ActiveDialogueWidget != nullptr;
}

void ANovaClickMovePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Display, TEXT("NOVA PC SetupInputComponent: %s InputComponent=%s"),
		*GetClass()->GetName(),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	// 고정 조작(QWER/F/G/Space/RMB/A/D)은 InputKey에서 처리. 음성·디버그 키만 BindKey.
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ANovaClickMovePlayerController::OnWeaponKey4);

	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF5);
	InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF6);
	InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF7);
	InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ANovaClickMovePlayerController::OnDebugCounterKeyF8);

	// 마우스/기본 Look 축 차단 (우클릭 이동만, 시점은 A/D)
	InputComponent->BindAxis(TEXT("Turn"), this, &ANovaClickMovePlayerController::ConsumeLookAxis);
	InputComponent->BindAxis(TEXT("LookUp"), this, &ANovaClickMovePlayerController::ConsumeLookAxis);
	InputComponent->BindAxis(TEXT("MouseX"), this, &ANovaClickMovePlayerController::ConsumeLookAxis);
	InputComponent->BindAxis(TEXT("MouseY"), this, &ANovaClickMovePlayerController::ConsumeLookAxis);
}

void ANovaClickMovePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsDialogueActive())
	{
		TickDialogueInput();
		return;
	}

	ApplyCameraYaw(DeltaTime);
	ApplyFixedOrbitCamera();

	// 우클릭 홀드: 커서 아래 목적지 갱신
	if (ControlMode == ENovaControlMode::ClickMove && bIsHoldingMove && !IsDialogueActive())
	{
		UpdateDestinationUnderCursor(/*bPrintDebug*/ false);
	}

	if (!bHasDestination || IsDialogueActive())
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
	const FVector ActiveTarget = GetActiveMoveTarget();
	FVector To = ActiveTarget - Current;

	const float Acceptance = (FMath::Abs(To.Z) > 30.f)
		? AcceptanceRadius * 1.75f
		: AcceptanceRadius;
	const float DistSq = To.SizeSquared();
	if (DistSq <= FMath::Square(Acceptance))
	{
		if (NavPathIndex + 1 < NavPathPoints.Num())
		{
			++NavPathIndex;
		}
		else
		{
			bHasDestination = false;
			NavPathPoints.Reset();
			NavPathIndex = 0;
			bDestinationRequiresDirectMove = false;
		}
		return;
	}

	To.Z = 0.f;
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
		NavPathPoints.Reset();
		NavPathIndex = 0;
		bDestinationRequiresDirectMove = false;
	}
}

void ANovaClickMovePlayerController::OnRightClickPressed()
{
	if (ControlMode != ENovaControlMode::ClickMove || IsDialogueActive())
	{
		return;
	}

	bIsHoldingMove = true;
	UpdateDestinationUnderCursor(/*bPrintDebug*/ false);
}

void ANovaClickMovePlayerController::OnRightClickReleased()
{
	bIsHoldingMove = false;
}

static USpringArmComponent* FindSpringArmOnPawn(APawn* P);
static UCameraComponent* FindCameraOnPawn(APawn* P);

void ANovaClickMovePlayerController::UpdateDestinationUnderCursor(bool bPrintDebug)
{
	FHitResult Hit;
	if (!GetWalkableHitUnderCursor(Hit))
	{
		if (bPrintDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, TEXT("바닥만 클릭 이동 가능"));
		}
		return;
	}

	Destination = Hit.Location;
	bDestinationRequiresDirectMove = IsStairSurfaceHit(Hit);

	if (!bDestinationRequiresDirectMove && GetPawn())
	{
		const float ZDelta = FMath::Abs(Destination.Z - GetPawn()->GetActorLocation().Z);
		if (ZDelta > 40.f)
		{
			bDestinationRequiresDirectMove = true;
		}
	}

	if (!bDestinationRequiresDirectMove)
	{
		const FVector PreSnapDestination = Destination;
		SnapDestinationToNavMesh(Destination);
		if (FVector::Dist(Destination, PreSnapDestination) > 120.f
			|| FMath::Abs(Destination.Z - PreSnapDestination.Z) > 50.f)
		{
			Destination = PreSnapDestination;
			bDestinationRequiresDirectMove = true;
		}
	}

	bHasDestination = true;
	RefreshNavPath();

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

bool ANovaClickMovePlayerController::IsWalkableSurfaceHit(const FHitResult& Hit, const float MinNormalZ)
{
	const AActor* Actor = Hit.GetActor();
	if (Actor)
	{
		const FString ActorName = Actor->GetName();
		const FString ClassName = Actor->GetClass()->GetName();

		auto NameHas = [](const FString& Haystack, const TCHAR* Needle) -> bool
		{
			return Haystack.Contains(Needle, ESearchCase::IgnoreCase);
		};

		if (NameHas(ActorName, TEXT("Wall"))
			|| NameHas(ActorName, TEXT("Railing"))
			|| NameHas(ActorName, TEXT("WallCover"))
			|| NameHas(ActorName, TEXT("WallTrim"))
			|| NameHas(ActorName, TEXT("Ceiling")))
		{
			return false;
		}

		const bool bNamedFloor = NameHas(ActorName, TEXT("Floor"))
			|| NameHas(ActorName, TEXT("MOD_Floor"))
			|| NameHas(ActorName, TEXT("MOD_Stairs"))
			|| NameHas(ActorName, TEXT("Ground"))
			|| NameHas(ActorName, TEXT("Terrain"))
			|| NameHas(ActorName, TEXT("Tile"))
			|| NameHas(ActorName, TEXT("Stair"))
			|| NameHas(ActorName, TEXT("Step"))
			|| NameHas(ClassName, TEXT("Floor"))
			|| NameHas(ClassName, TEXT("Stair"));

		const float MinNormal = bNamedFloor ? FMath::Min(MinNormalZ, 0.42f) : MinNormalZ;

		if (!bNamedFloor)
		{
			if (NameHas(ActorName, TEXT("Door"))
				|| NameHas(ActorName, TEXT("Arch"))
				|| NameHas(ActorName, TEXT("Chain"))
				|| NameHas(ActorName, TEXT("Column"))
				|| NameHas(ActorName, TEXT("Pillar"))
				|| NameHas(ActorName, TEXT("Roof"))
				|| NameHas(ActorName, TEXT("Beam"))
				|| (NameHas(ClassName, TEXT("COMP")) && !NameHas(ActorName, TEXT("Stair")))
				|| (NameHas(ActorName, TEXT("COMP")) && !NameHas(ActorName, TEXT("Stair"))))
			{
				return false;
			}
		}

		if (Hit.Normal.Z >= MinNormal)
		{
			return true;
		}

		if (bNamedFloor)
		{
			return true;
		}
	}
	else if (Hit.Normal.Z >= MinNormalZ)
	{
		return true;
	}

	if (Actor)
	{
		const FString ActorName = Actor->GetName();
		const FString ClassName = Actor->GetClass()->GetName();
		auto NameHas = [](const FString& Haystack, const TCHAR* Needle) -> bool
		{
			return Haystack.Contains(Needle, ESearchCase::IgnoreCase);
		};

		if (NameHas(ActorName, TEXT("Floor"))
			|| NameHas(ActorName, TEXT("MOD_Floor"))
			|| NameHas(ActorName, TEXT("Ground"))
			|| NameHas(ActorName, TEXT("Terrain"))
			|| NameHas(ClassName, TEXT("Floor")))
		{
			return true;
		}
	}

	return false;
}

bool ANovaClickMovePlayerController::IsStairSurfaceHit(const FHitResult& Hit)
{
	const AActor* Actor = Hit.GetActor();
	if (!Actor)
	{
		return false;
	}

	const FString ActorName = Actor->GetName();
	const FString ClassName = Actor->GetClass()->GetName();

	auto NameHas = [](const FString& Haystack, const TCHAR* Needle) -> bool
	{
		return Haystack.Contains(Needle, ESearchCase::IgnoreCase);
	};

	return NameHas(ActorName, TEXT("Stair"))
		|| NameHas(ActorName, TEXT("MOD_Stairs"))
		|| NameHas(ActorName, TEXT("stairs"))
		|| NameHas(ClassName, TEXT("Stair"))
		|| NameHas(ClassName, TEXT("stairs"));
}

bool ANovaClickMovePlayerController::ShouldUseNavMeshPath(
	const APawn* InPawn,
	const UNavigationSystemV1* NavSys) const
{
	if (!InPawn || !NavSys || bDestinationRequiresDirectMove)
	{
		return false;
	}

	FNavLocation PawnNav;
	FNavLocation DestNav;
	const FVector Extent(NavProjectionExtent, NavProjectionExtent, NavProjectionVerticalExtent);

	const bool bPawnOnNav = NavSys->ProjectPointToNavigation(InPawn->GetActorLocation(), PawnNav, Extent);
	const bool bDestOnNav = NavSys->ProjectPointToNavigation(Destination, DestNav, Extent);
	if (!bPawnOnNav || !bDestOnNav)
	{
		return false;
	}

	if (FMath::Abs(DestNav.Location.Z - Destination.Z) > 60.f)
	{
		return false;
	}

	return true;
}

bool ANovaClickMovePlayerController::ProjectCursorToWalkablePoint(FVector& InOutWorldPoint, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const APawn* P = GetPawn();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NovaClickMoveWalkableDown), false, P);
	const float ReferenceZ = P ? P->GetActorLocation().Z : InOutWorldPoint.Z;
	const float TraceTopZ = FMath::Max(InOutWorldPoint.Z, ReferenceZ) + ClickMoveVerticalTraceUp;
	const FVector TraceStart(InOutWorldPoint.X, InOutWorldPoint.Y, TraceTopZ);
	const FVector TraceEnd(InOutWorldPoint.X, InOutWorldPoint.Y, InOutWorldPoint.Z - ClickMoveVerticalTraceDown);

	auto TraceDownOnChannel = [&](const ECollisionChannel Channel) -> bool
	{
		TArray<FHitResult> Hits;
		if (!World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, Channel, Params))
		{
			return false;
		}

		Hits.Sort([](const FHitResult& A, const FHitResult& B)
		{
			return A.Distance < B.Distance;
		});

		for (const FHitResult& Hit : Hits)
		{
			if (IsWalkableSurfaceHit(Hit, WalkableMinNormalZ))
			{
				OutHit = Hit;
				InOutWorldPoint = Hit.Location;
				return true;
			}
		}

		return false;
	};

	return TraceDownOnChannel(ECC_WorldStatic)
		|| TraceDownOnChannel(ECC_Visibility)
		|| TraceDownOnChannel(ECC_Camera);
}

bool ANovaClickMovePlayerController::FindWalkableHitOnCameraRay(const FVector& PlanePoint, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const APawn* P = GetPawn();
	if (!World)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	const FVector TraceEnd = WorldOrigin + WorldDirection * ClickMoveTraceDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NovaClickMoveWalkableCam), false, P);

	float BestRayDistance = TNumericLimits<float>::Max();
	bool bFound = false;

	auto TraceChannel = [&](const ECollisionChannel Channel) -> void
	{
		TArray<FHitResult> Hits;
		if (!World->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, Channel, Params))
		{
			return;
		}

		Hits.Sort([](const FHitResult& A, const FHitResult& B)
		{
			return A.Distance < B.Distance;
		});

		for (const FHitResult& Hit : Hits)
		{
			if (!IsWalkableSurfaceHit(Hit, WalkableMinNormalZ))
			{
				continue;
			}

			const float XYSlop = FVector2D::Distance(
				FVector2D(Hit.ImpactPoint.X, Hit.ImpactPoint.Y),
				FVector2D(PlanePoint.X, PlanePoint.Y));
			if (XYSlop > ClickMoveMaxXYSlop)
			{
				continue;
			}

			if (Hit.Distance < BestRayDistance)
			{
				BestRayDistance = Hit.Distance;
				OutHit = Hit;
				bFound = true;
			}
		}
	};

	TraceChannel(ECC_WorldStatic);
	TraceChannel(ECC_Visibility);
	TraceChannel(ECC_Camera);

	return bFound;
}

bool ANovaClickMovePlayerController::GetWalkableHitUnderCursor(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const APawn* P = GetPawn();
	if (!World)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	if (FMath::IsNearlyZero(WorldDirection.Z))
	{
		return false;
	}

	const float PlaneZ = P ? P->GetActorLocation().Z : WorldOrigin.Z;
	const float T = (PlaneZ - WorldOrigin.Z) / WorldDirection.Z;
	if (T <= 0.f)
	{
		return false;
	}

	const FVector PlanePoint = WorldOrigin + WorldDirection * T;

	// 계단·상층 바닥: 카메라 레이로 보이는 면 우선 (수직 트레이스만 쓰면 아래층 바닥이 잡힘)
	if (FindWalkableHitOnCameraRay(PlanePoint, OutHit))
	{
		return true;
	}

	FVector FallbackPoint = PlanePoint;
	return ProjectCursorToWalkablePoint(FallbackPoint, OutHit);
}

bool ANovaClickMovePlayerController::SnapDestinationToNavMesh(FVector& InOutLocation) const
{
	const UWorld* World = GetWorld();
	if (!World || !bUseNavMeshPathfinding)
	{
		return true;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return true;
	}

	FNavLocation Projected;
	const FVector Extent(NavProjectionExtent, NavProjectionExtent, NavProjectionVerticalExtent);
	if (NavSys->ProjectPointToNavigation(InOutLocation, Projected, Extent))
	{
		InOutLocation = Projected.Location;
	}

	return true;
}

void ANovaClickMovePlayerController::RefreshNavPath()
{
	NavPathPoints.Reset();
	NavPathIndex = 0;

	const APawn* P = GetPawn();
	UWorld* World = GetWorld();
	if (!P || !World || !bUseNavMeshPathfinding)
	{
		NavPathPoints.Add(Destination);
		return;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys || !ShouldUseNavMeshPath(P, NavSys))
	{
		NavPathPoints.Add(Destination);
		return;
	}

	FNavLocation DestNav;
	const FVector Extent(NavProjectionExtent, NavProjectionExtent, NavProjectionVerticalExtent);
	if (!NavSys->ProjectPointToNavigation(Destination, DestNav, Extent))
	{
		NavPathPoints.Add(Destination);
		return;
	}

	UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
		World,
		P->GetActorLocation(),
		DestNav.Location,
		const_cast<APawn*>(P)
	);

	if (Path && Path->IsValid() && Path->PathPoints.Num() > 1)
	{
		const FVector& PathEnd = Path->PathPoints.Last();
		if (FVector::Dist(PathEnd, Destination) > 150.f
			|| FMath::Abs(PathEnd.Z - Destination.Z) > 80.f)
		{
			NavPathPoints.Add(Destination);
			return;
		}

		NavPathPoints = Path->PathPoints;
		NavPathIndex = 1;
	}
	else
	{
		NavPathPoints.Add(Destination);
	}
}

FVector ANovaClickMovePlayerController::GetActiveMoveTarget() const
{
	if (NavPathPoints.IsValidIndex(NavPathIndex))
	{
		return NavPathPoints[NavPathIndex];
	}

	return Destination;
}

void ANovaClickMovePlayerController::ApplyFixedOrbitCamera()
{
	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	USpringArmComponent* Arm = FindSpringArmOnPawn(P);
	UCameraComponent* Cam = FindCameraOnPawn(P);

	if (Arm)
	{
		// 시점 고정: 충돌·건축물·몹 근접 시 Arm 길이/각도가 변하지 않게
		Arm->bDoCollisionTest = false;
		Arm->bEnableCameraLag = false;
		Arm->bUsePawnControlRotation = false;
		Arm->bInheritPitch = false;
		Arm->bInheritRoll = false;
		Arm->bInheritYaw = false;
		Arm->TargetArmLength = FixedOrbitArmLength;
		Arm->SetRelativeRotation(FRotator(FixedOrbitPitch, CameraYawDegrees, 0.f));
	}

	if (Cam)
	{
		Cam->bUsePawnControlRotation = false;
	}

	const float FacingYaw = FMath::UnwindDegrees(CameraYawDegrees + CharacterFacingYawOffset);
	P->SetActorRotation(FRotator(0.f, FacingYaw, 0.f));

	if (ACharacter* C = Cast<ACharacter>(P))
	{
		if (USkeletalMeshComponent* SkMesh = C->GetMesh())
		{
			FRotator MeshRel = SkMesh->GetRelativeRotation();
			if (!FMath::IsNearlyZero(MeshRel.Yaw))
			{
				MeshRel.Yaw = 0.f;
				SkMesh->SetRelativeRotation(MeshRel);
			}
		}
	}

	SetControlRotation(FRotator(0.f, 0.f, 0.f));
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

void ANovaClickMovePlayerController::OnJumpPressed()
{
	if (IsDialogueActive())
	{
		return;
	}

	if (ACharacter* C = Cast<ACharacter>(GetPawn()))
	{
		C->Jump();
	}
}

void ANovaClickMovePlayerController::OnDashPressed()
{
	if (IsDialogueActive())
	{
		return;
	}

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
	ApplyWeaponVisualToPawn(WeaponCommand);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Green,
			FString::Printf(TEXT("Weapon switched: %s"), *NovaWeaponDisplay::CommandToDisplayName(WeaponCommand))
		);
	}

	OnSecondaryWeaponChanged(WeaponCommand);
	return true;
}

void ANovaClickMovePlayerController::ApplyWeaponVisualToPawn(ENovaVoiceCommand WeaponCommand)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || !ControlledPawn->GetWorld())
	{
		return;
	}

	UNovaSecondaryWeaponVisualComponent* VisualComponent =
		ControlledPawn->FindComponentByClass<UNovaSecondaryWeaponVisualComponent>();
	if (!VisualComponent)
	{
		VisualComponent = Cast<UNovaSecondaryWeaponVisualComponent>(
			ControlledPawn->AddComponentByClass(
				UNovaSecondaryWeaponVisualComponent::StaticClass(),
				/*bManualAttachment*/ false,
				FTransform::Identity,
				/*bDeferredFinish*/ false));
	}

	if (VisualComponent)
	{
		VisualComponent->SetVisibleWeapon(WeaponCommand);
	}
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
	if (!CommandResult.bAccepted)
	{
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

	if (CombatVoiceGateComponent)
	{
		FString CounterRejectReason;
		if (!CombatVoiceGateComponent->TryResolveCounterWindow(CommandResult, CounterRejectReason))
		{
			if (GEngine && !CounterRejectReason.IsEmpty())
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, CounterRejectReason);
			}
		}
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

	OnCompanionHelpVisualRequested();
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
				*NovaWeaponDisplay::CommandToDisplayName(Command)
			)
		);
	}

	OnBossCounterVisualSuccess(CounterType, Command);
}

void ANovaClickMovePlayerController::OnWeaponKey1()
{
	HandleWeaponSwitchInput(ENovaVoiceCommand::Hammer); // 검
}

void ANovaClickMovePlayerController::OnWeaponKey2()
{
	HandleWeaponSwitchInput(ENovaVoiceCommand::Bow); // 활
}

void ANovaClickMovePlayerController::OnWeaponKey3()
{
	HandleWeaponSwitchInput(ENovaVoiceCommand::Scythe); // 창
}

void ANovaClickMovePlayerController::OnWeaponKey4()
{
	HandleWeaponSwitchInput(ENovaVoiceCommand::Shield); // 방패
}

void ANovaClickMovePlayerController::OnDebugCounterKeyF5() { OpenBossCounterWindow(ENovaBossCounterType::LaserShield); }
void ANovaClickMovePlayerController::OnDebugCounterKeyF6() { OpenBossCounterWindow(ENovaBossCounterType::SpaceScythe); }
void ANovaClickMovePlayerController::OnDebugCounterKeyF7() { OpenBossCounterWindow(ENovaBossCounterType::SummonBow); }
void ANovaClickMovePlayerController::OnDebugCounterKeyF8() { OpenBossCounterWindow(ENovaBossCounterType::DebrisHammer); }


void ANovaClickMovePlayerController::OnSkillQ()
{
	if (IsDialogueActive()) { return; }
	BP_UseSkillQ(EquippedSecondaryWeapon);
}

void ANovaClickMovePlayerController::OnSkillW()
{
	if (IsDialogueActive()) { return; }
	BP_UseSkillW(EquippedSecondaryWeapon);
}

void ANovaClickMovePlayerController::OnSkillE()
{
	if (IsDialogueActive()) { return; }
	BP_UseSkillE(EquippedSecondaryWeapon);
}

void ANovaClickMovePlayerController::OnSkillR()
{
	if (IsDialogueActive()) { return; }
	BP_UseSkillR(EquippedSecondaryWeapon);
}

bool ANovaClickMovePlayerController::IsFirstPersonDungeonPawn() const
{
	const APawn* P = GetPawn();
	if (!P)
	{
		return false;
	}

	const FString ClassName = P->GetClass()->GetName();
	return ClassName.Contains(TEXT("FirstPersonCharacter"));
}

bool ANovaClickMovePlayerController::InputKey(const FInputKeyEventArgs& EventArgs)
{
	const FKey& Key = EventArgs.Key;

	// 마우스 이동/휠로 시점이 돌아가지 않도록 차단
	if (Key == EKeys::MouseX || Key == EKeys::MouseY || Key == EKeys::Mouse2D
		|| Key == EKeys::MouseWheelAxis)
	{
		return true;
	}

	if (Key == EKeys::RightMouseButton)
	{
		if (EventArgs.Event == IE_Pressed)
		{
			OnRightClickPressed();
		}
		else if (EventArgs.Event == IE_Released)
		{
			OnRightClickReleased();
		}
		return true;
	}

	if (Key == EKeys::LeftMouseButton && EventArgs.Event == IE_Pressed)
	{
		OnDashPressed();
		return true;
	}

	// A/D: 시점 전용 — PlayerTick에서 bCameraYawLeft/Right로 회전
	if (Key == EKeys::A)
	{
		if (EventArgs.Event == IE_Pressed)
		{
			bCameraYawLeft = true;
		}
		else if (EventArgs.Event == IE_Released)
		{
			bCameraYawLeft = false;
		}
		return true;
	}
	if (Key == EKeys::D)
	{
		if (EventArgs.Event == IE_Pressed)
		{
			bCameraYawRight = true;
		}
		else if (EventArgs.Event == IE_Released)
		{
			bCameraYawRight = false;
		}
		return true;
	}

	if (EventArgs.Event == IE_Pressed)
	{
		if (Key == EKeys::Q) { OnSkillQ(); return true; }
		if (Key == EKeys::W) { OnSkillW(); return true; }
		if (Key == EKeys::E) { OnSkillE(); return true; }
		if (Key == EKeys::R) { OnSkillR(); return true; }
		if (Key == EKeys::F) { TryStartNpcDialogue(); return true; }
		if (Key == EKeys::G) { EndDialogueMode(); return true; }
		if (Key == EKeys::SpaceBar) { OnJumpPressed(); return true; }

		// Pawn 기본 입력(이동/점프) 차단 — Q/W/E/R/F/G/Space/A/D/RMB는 위에서 처리
		if (Key == EKeys::S || Key == EKeys::Up || Key == EKeys::Down
			|| Key == EKeys::Left || Key == EKeys::Right
			|| Key == EKeys::LeftShift || Key == EKeys::LeftControl)
		{
			return true;
		}
	}

	return Super::InputKey(EventArgs);
}

void ANovaClickMovePlayerController::TryStartNpcDialogue()
{
	if (IsDialogueActive())
	{
		return;
	}

	APawn* P = GetPawn();
	if (!P || !GetWorld())
	{
		return;
	}

	const FVector Origin = P->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NovaNpcInteract), false, P);

	TArray<FOverlapResult> Overlaps;
	if (GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(NpcInteractRadius),
		Params))
	{
		AActor* ClosestNpc = nullptr;
		float ClosestDistSq = TNumericLimits<float>::Max();

		for (const FOverlapResult& Result : Overlaps)
		{
			AActor* Candidate = Result.GetActor();
			if (!Candidate || Candidate == P)
			{
				continue;
			}

			const FString ClassName = Candidate->GetClass()->GetName();
			if (!ClassName.Contains(TEXT("bp_npc"), ESearchCase::IgnoreCase)
				&& !ClassName.Contains(TEXT("NPC"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				ClosestNpc = Candidate;
			}
		}

		if (ClosestNpc)
		{
			StartDialogueMode(ClosestNpc);
			return;
		}
	}

	FVector TraceStart = Origin + FVector(0.f, 0.f, 50.f);
	FVector TraceEnd = TraceStart + P->GetActorForwardVector() * NpcInteractRadius;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor)
		{
			const FString ClassName = HitActor->GetClass()->GetName();
			if (ClassName.Contains(TEXT("bp_npc"), ESearchCase::IgnoreCase)
				|| ClassName.Contains(TEXT("NPC"), ESearchCase::IgnoreCase))
			{
				StartDialogueMode(HitActor);
				return;
			}
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Orange, TEXT("근처 NPC 없음 (F)"));
	}
}

void ANovaClickMovePlayerController::TurnCamera(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	CameraYawDegrees = FMath::UnwindDegrees(CameraYawDegrees + AxisValue);
}

void ANovaClickMovePlayerController::ApplyCameraYaw(float DeltaTime)
{
	float TurnAxis = 0.0f;
	if (bCameraYawLeft)
	{
		TurnAxis -= 1.0f;
	}
	if (bCameraYawRight)
	{
		TurnAxis += 1.0f;
	}

	if (!FMath::IsNearlyZero(TurnAxis))
	{
		TurnCamera(TurnAxis * CameraTurnSpeed * DeltaTime);
	}
}

void ANovaClickMovePlayerController::StartDialogueMode(AActor* NpcActor)
{
	if (!DialogueWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nova StartDialogueMode: DialogueWidgetClass missing"));
		return;
	}

	ActiveDialogueNpc = NpcActor;

	if (!ActiveDialogueWidget)
	{
		ActiveDialogueWidget = CreateWidget<UUserWidget>(this, DialogueWidgetClass);
		if (ActiveDialogueWidget)
		{
			ActiveDialogueWidget->AddToViewport(100);
		}
	}

	if (!ActiveDialogueWidget)
	{
		return;
	}

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	ApplyGameplayInputMode();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("NPC 대화 시작 (G로 종료)"));
	}
}

void ANovaClickMovePlayerController::EndDialogueMode()
{
	const bool bWasActive = IsDialogueActive();

	if (ActiveDialogueWidget)
	{
		ActiveDialogueWidget->RemoveFromParent();
		ActiveDialogueWidget = nullptr;
	}

	ActiveDialogueNpc = nullptr;
	bIsHoldingMove = false;
	bPrevGKeyDown = false;
	bCameraYawLeft = false;
	bCameraYawRight = false;
	SetIgnoreMoveInput(false);
	ApplyGameplayInputMode();

	if (bWasActive && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("NPC 대화 종료"));
	}
}
