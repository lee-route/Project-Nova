#include "NovaGameMode.h"

#include "UObject/ConstructorHelpers.h"
#include "NovaClickMovePlayerController.h"

ANovaGameMode::ANovaGameMode()
{
	// Keep using the existing template Blueprint character.
	static ConstructorHelpers::FClassFinder<APawn> DefaultPawnBP(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter")
	);
	if (DefaultPawnBP.Succeeded())
	{
		DefaultPawnClass = DefaultPawnBP.Class;
	}

	// Prefer the Blueprint child so we can set click-move indicator FX in BP.
	static ConstructorHelpers::FClassFinder<APlayerController> NovaPCBp(
		TEXT("/Game/ThirdPerson/Blueprints/BP_NovaPlayerController")
	);
	if (NovaPCBp.Succeeded())
	{
		PlayerControllerClass = NovaPCBp.Class;
	}
	else
	{
		PlayerControllerClass = ANovaClickMovePlayerController::StaticClass();
	}
}

