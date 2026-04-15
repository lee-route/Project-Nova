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
	ANovaCharacter();

protected:
	virtual void BeginPlay() override;

	// Fixed 3rd-person top-down camera (Diablo-like).
	UPROPERTY(VisibleAnywhere, Category = Camera)
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	UCameraComponent* Camera = nullptr;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};

