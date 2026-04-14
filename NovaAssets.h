#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace NovaAssets
{
	// Shared asset getter so Pawn/Character both resolve the same mesh.
	inline USkeletalMesh* GetCardboardMesh()
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> Finder(
			TEXT("/Game/InfinityBladeWarriors/Character/CompleteCharacters/SK_CharM_Cardboard.SK_CharM_Cardboard")
		);

		return Finder.Succeeded() ? Finder.Object : nullptr;
	}
}

