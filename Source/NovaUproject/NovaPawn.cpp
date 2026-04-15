#include "NovaPawn.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

ANovaPawn::ANovaPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CAPSULE"));
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MESH"));
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MOVEMENT"));
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SPRINGARM"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CAMERA"));

	RootComponent = Capsule;
	Mesh->SetupAttachment(Capsule);
	SpringArm->SetupAttachment(Capsule);
	Camera->SetupAttachment(SpringArm);

	Capsule->SetCapsuleHalfHeight(88.0f);
	Capsule->SetCapsuleRadius(34.0f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));

	// Move/collision should be driven by the capsule, not the mesh.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Movement->UpdatedComponent = Capsule;

	Mesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -88.0f), FRotator(0.0f, -90.0f, 0.0f));

	SpringArm->TargetArmLength = 400.0f;
	SpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bDoCollisionTest = true;
	SpringArm->ProbeChannel = ECC_Camera;
	SpringArm->ProbeSize = 12.0f;

	Camera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = true;

	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	static ConstructorHelpers::FClassFinder<UAnimInstance> WARRIOR_ANIM(
		TEXT("/Game/Book/Animations/WarriorAnimBlueprint.WarriorAnimBlueprint_C")
	);
	if (WARRIOR_ANIM.Succeeded())
	{
		Mesh->SetAnimInstanceClass(WARRIOR_ANIM.Class);
	}
}

void ANovaPawn::BeginPlay()
{
	Super::BeginPlay();
}

void ANovaPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ANovaPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Legacy input bindings (Project Settings -> Input)
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ANovaPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ANovaPawn::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ANovaPawn::Turn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ANovaPawn::LookUp);
}

void ANovaPawn::MoveForward(float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value))
	{
		return;
	}

	// Camera/control-rotation based 360 movement (yaw only).
	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	AddMovementInput(Forward, Value);
}

void ANovaPawn::MoveRight(float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value))
	{
		return;
	}

	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	AddMovementInput(Right, Value);
}

void ANovaPawn::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ANovaPawn::LookUp(float Value)
{
	if (!Controller || FMath::IsNearlyZero(Value))
	{
		return;
	}

	// Prevent pitching so far that the camera can clip through the floor.
	FRotator Rot = Controller->GetControlRotation();
	Rot.Pitch = FMath::ClampAngle(Rot.Pitch + Value, -80.0f, -10.0f);
	Controller->SetControlRotation(Rot);
}

