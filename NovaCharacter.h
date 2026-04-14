// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NovaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class NOVAUPROJECT_API ANovaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ANovaCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Fixed 3rd-person top-down camera (Diablo-like). No camera mode switching.
	UPROPERTY(VisibleAnywhere, Category = Camera)
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	UCameraComponent* Camera = nullptr;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	// Click-to-move is handled by the PlayerController.
};
