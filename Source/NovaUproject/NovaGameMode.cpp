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

	PlayerControllerClass = ANovaClickMovePlayerController::StaticClass();
}

