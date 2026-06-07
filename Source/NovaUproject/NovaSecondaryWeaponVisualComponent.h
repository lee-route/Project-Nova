#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "NovaVoiceTypes.h"
#include "NovaSecondaryWeaponVisualComponent.generated.h"

class UStaticMeshComponent;

UCLASS(ClassGroup = (Nova), meta = (BlueprintSpawnableComponent))
class NOVAUPROJECT_API UNovaSecondaryWeaponVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNovaSecondaryWeaponVisualComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FName AttachSocketName = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FVector AttachLocationOffset = FVector(10.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|Visual")
	ENovaVoiceCommand VisibleWeapon = ENovaVoiceCommand::None;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Visual")
	void SetVisibleWeapon(ENovaVoiceCommand NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Visual")
	ENovaVoiceCommand GetVisibleWeapon() const { return VisibleWeapon; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Visual")
	void OnWeaponVisualChanged(ENovaVoiceCommand NewWeapon);

private:
	UPROPERTY(Transient)
	TMap<ENovaVoiceCommand, TObjectPtr<UStaticMeshComponent>> WeaponMeshComponents;

	void EnsureWeaponMeshesCreated();
	UStaticMeshComponent* CreatePlaceholderWeaponMesh(ENovaVoiceCommand Weapon, UStaticMesh* Mesh, const FVector& Scale, const FRotator& Rotation);
};
