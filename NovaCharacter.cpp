// Fill out your copyright notice in the Description page of Project Settings.


#include "NovaCharacter.h"
#include "NovaAssets.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

// Sets default values
ANovaCharacter::ANovaCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Diablo-like fixed top-down camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SPRINGARM"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->TargetArmLength = 800.0f;
	SpringArm->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = false;

	// No camera zoom/pull-in. Keep a constant 800cm distance.
	// Occlusion is handled by outlining the character instead.
	SpringArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("CAMERA"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Movement: 2D on ground, orient to movement direction.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// Click-to-move: rotate character toward velocity.
		CMC->bOrientRotationToMovement = true;
		CMC->bUseControllerDesiredRotation = false;
		CMC->RotationRate = FRotator(0.f, 720.f, 0.f);
		CMC->bConstrainToPlane = true;
		CMC->SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting::Z);
	}

	// Match common mannequin-style offset so the mesh sits on the capsule.
	GetCapsuleComponent()->SetCapsuleHalfHeight(88.0f);
	GetCapsuleComponent()->SetCapsuleRadius(34.0f);

	GetMesh()->SetRelativeLocationAndRotation(
		FVector(0.0f, 0.0f, -88.0f),
		FRotator(0.0f, -90.0f, 0.0f)
	);

	if (USkeletalMesh* CardboardMesh = NovaAssets::GetCardboardMesh())
	{
		GetMesh()->SetSkeletalMesh(CardboardMesh);
	}
}

// Called when the game starts or when spawned
void ANovaCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ANovaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ANovaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}