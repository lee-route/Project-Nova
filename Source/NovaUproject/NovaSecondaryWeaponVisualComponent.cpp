#include "NovaSecondaryWeaponVisualComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"

namespace
{
	UStaticMesh* LoadEngineMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	FName ResolveWeaponAttachSocket(USkeletalMeshComponent* SkMesh)
	{
		if (!SkMesh)
		{
			return NAME_None;
		}

		static const FName SocketCandidates[] = {
			TEXT("hand_r"),
			TEXT("Hand_R"),
			TEXT("hand_R"),
			TEXT("RightHand"),
			TEXT("WeaponSocket"),
			TEXT("weapon_r"),
		};

		for (const FName SocketName : SocketCandidates)
		{
			if (SkMesh->DoesSocketExist(SocketName))
			{
				return SocketName;
			}
		}

		return NAME_None;
	}
}

UNovaSecondaryWeaponVisualComponent::UNovaSecondaryWeaponVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNovaSecondaryWeaponVisualComponent::EnsureWeaponMeshesCreated()
{
	if (WeaponMeshComponents.Num() > 0)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetWorld())
	{
		return;
	}

	UStaticMesh* CubeMesh = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylinderMesh = LoadEngineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!CubeMesh)
	{
		return;
	}

	CreatePlaceholderWeaponMesh(ENovaVoiceCommand::Hammer, CubeMesh, FVector(0.08f, 0.02f, 0.5f), FRotator(0.0f, 0.0f, 90.0f));
	CreatePlaceholderWeaponMesh(ENovaVoiceCommand::Bow, CylinderMesh ? CylinderMesh : CubeMesh, FVector(0.05f, 0.05f, 0.45f), FRotator(0.0f, 90.0f, 0.0f));
	CreatePlaceholderWeaponMesh(ENovaVoiceCommand::Spear, CubeMesh, FVector(0.06f, 0.03f, 0.65f), FRotator(0.0f, 0.0f, 110.0f));
	CreatePlaceholderWeaponMesh(ENovaVoiceCommand::Shield, CubeMesh, FVector(0.45f, 0.06f, 0.55f), FRotator(0.0f, 0.0f, 0.0f));

	for (const TPair<ENovaVoiceCommand, TObjectPtr<UStaticMeshComponent>>& Pair : WeaponMeshComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->SetVisibility(false);
		}
	}
}

UStaticMeshComponent* UNovaSecondaryWeaponVisualComponent::CreatePlaceholderWeaponMesh(
	ENovaVoiceCommand Weapon,
	UStaticMesh* Mesh,
	const FVector& Scale,
	const FRotator& Rotation)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Mesh)
	{
		return nullptr;
	}

	USceneComponent* AttachParent = Owner->GetRootComponent();
	FName SocketName = NAME_None;
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (USkeletalMeshComponent* SkMesh = Character->GetMesh())
		{
			if (SkMesh->IsRegistered())
			{
				AttachParent = SkMesh;
				SocketName = ResolveWeaponAttachSocket(SkMesh);
			}
		}
	}

	if (!AttachParent || !AttachParent->IsRegistered())
	{
		return nullptr;
	}

	const FVector LocationOffset = SocketName.IsNone()
		? FVector(30.f, 15.f, 0.f)
		: AttachLocationOffset;

	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(
		Owner,
		UStaticMeshComponent::StaticClass(),
		*FString::Printf(TEXT("WeaponMesh_%d"), static_cast<int32>(Weapon)));
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetRelativeScale3D(Scale);
	MeshComponent->SetRelativeRotation(Rotation);
	MeshComponent->SetRelativeLocation(LocationOffset);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetupAttachment(AttachParent, SocketName);
	Owner->AddInstanceComponent(MeshComponent);
	MeshComponent->RegisterComponent();

	WeaponMeshComponents.Add(Weapon, MeshComponent);
	return MeshComponent;
}

void UNovaSecondaryWeaponVisualComponent::SetVisibleWeapon(ENovaVoiceCommand NewWeapon)
{
	if (NewWeapon != ENovaVoiceCommand::Bow
		&& NewWeapon != ENovaVoiceCommand::Shield
		&& NewWeapon != ENovaVoiceCommand::Spear
		&& NewWeapon != ENovaVoiceCommand::Hammer)
	{
		return;
	}

	EnsureWeaponMeshesCreated();

	for (const TPair<ENovaVoiceCommand, TObjectPtr<UStaticMeshComponent>>& Pair : WeaponMeshComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->SetVisibility(Pair.Key == NewWeapon);
		}
	}

	VisibleWeapon = NewWeapon;
	OnWeaponVisualChanged(NewWeapon);
}
