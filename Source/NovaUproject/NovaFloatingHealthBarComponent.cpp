#include "NovaFloatingHealthBarComponent.h"

#include "NovaMonsterHealthBarWidget.h"
#include "NovaParagonGruxBossComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UNovaFloatingHealthBarComponent::UNovaFloatingHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UUserWidget> WidgetBPClassFinder(
		TEXT("/Game/UI/WBP_MonsterHealthBar.WBP_MonsterHealthBar_C"));
	if (WidgetBPClassFinder.Succeeded())
	{
		WidgetClass = WidgetBPClassFinder.Class;
	}
	else
	{
		WidgetClass = UNovaMonsterHealthBarWidget::StaticClass();
	}
}

void UNovaFloatingHealthBarComponent::ApplyBossHealthBarPreset(const float InMaxHealth)
{
	MaxHealth = InMaxHealth;
	CurrentHealth = InMaxHealth;
	BarDrawSize = FVector2D(180.0f, 16.0f);
	BarRelativeLocation = FVector(0.0f, 0.0f, 220.0f);
	RefreshHealthBar();
}

void UNovaFloatingHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UNovaFloatingHealthBarComponent::HandleAnyDamage);
	}

	EnsureWidgetComponent();
	CachedWidgetComponent = ResolveWidgetComponent();
	InitializeHealth();
}

float UNovaFloatingHealthBarComponent::GetHealthPercent() const
{
	if (MaxHealth <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f);
}

void UNovaFloatingHealthBarComponent::InitializeHealth()
{
	CurrentHealth = MaxHealth;
	RefreshHealthBar();
}

void UNovaFloatingHealthBarComponent::ActivateHealthBar()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UNovaFloatingHealthBarComponent::HandleAnyDamage);
		Owner->OnTakeAnyDamage.AddDynamic(this, &UNovaFloatingHealthBarComponent::HandleAnyDamage);
	}

	EnsureWidgetComponent();
	CachedWidgetComponent = ResolveWidgetComponent();
	InitializeHealth();
}

void UNovaFloatingHealthBarComponent::RefreshHealthBar()
{
	ApplyHealthBarPercent(GetHealthPercent());
}

void UNovaFloatingHealthBarComponent::EnsureWidgetComponent()
{
	if (!bAutoCreateWidgetIfMissing || CachedWidgetComponent)
	{
		return;
	}

	if (ResolveWidgetComponent())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	USceneComponent* AttachParent = Owner->GetRootComponent();
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			AttachParent = Mesh;
		}
	}

	if (!AttachParent)
	{
		return;
	}

	UWidgetComponent* WidgetComponent = NewObject<UWidgetComponent>(
		Owner,
		UWidgetComponent::StaticClass(),
		WidgetComponentName);
	WidgetComponent->SetupAttachment(AttachParent);
	WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComponent->SetDrawAtDesiredSize(false);
	WidgetComponent->SetDrawSize(BarDrawSize);
	WidgetComponent->SetRelativeLocation(BarRelativeLocation);
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (WidgetClass)
	{
		WidgetComponent->SetWidgetClass(WidgetClass);
	}

	Owner->AddInstanceComponent(WidgetComponent);
	WidgetComponent->RegisterComponent();
	WidgetComponent->InitWidget();

	CachedWidgetComponent = WidgetComponent;
	bCreatedWidgetComponent = true;
}

UWidgetComponent* UNovaFloatingHealthBarComponent::ResolveWidgetComponent()
{
	if (CachedWidgetComponent)
	{
		return CachedWidgetComponent;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	Owner->GetComponents<UWidgetComponent>(WidgetComponents);

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (WidgetComponent && WidgetComponent->GetName().Contains(WidgetComponentName.ToString()))
		{
			return WidgetComponent;
		}
	}

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (WidgetComponent)
		{
			return WidgetComponent;
		}
	}

	return nullptr;
}

void UNovaFloatingHealthBarComponent::ApplyHealthBarPercent(const float Percent)
{
	if (!CachedWidgetComponent)
	{
		EnsureWidgetComponent();
		CachedWidgetComponent = ResolveWidgetComponent();
	}

	UWidgetComponent* WidgetComponent = CachedWidgetComponent;
	if (!WidgetComponent)
	{
		return;
	}

	WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComponent->SetRelativeLocation(BarRelativeLocation);
	WidgetComponent->SetDrawSize(BarDrawSize);
	WidgetComponent->SetVisibility(true);

	UUserWidget* Widget = WidgetComponent->GetUserWidgetObject();
	if (!Widget)
	{
		WidgetComponent->InitWidget();
		Widget = WidgetComponent->GetUserWidgetObject();
	}

	if (!Widget)
	{
		return;
	}

	if (UNovaMonsterHealthBarWidget* NovaWidget = Cast<UNovaMonsterHealthBarWidget>(Widget))
	{
		NovaWidget->UpdateHealth(Percent);
		return;
	}

	if (UFunction* UpdateHealthFunction = Widget->FindFunction(FName("UpdateHealth")))
	{
		struct FNovaUpdateHealthParams
		{
			float Percent = 0.0f;
		};

		FNovaUpdateHealthParams Params;
		Params.Percent = Percent;
		Widget->ProcessEvent(UpdateHealthFunction, &Params);
	}
}

void UNovaFloatingHealthBarComponent::HandleAnyDamage(
	AActor* DamagedActor,
	const float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	if (Damage <= 0.0f || CurrentHealth <= 0.0f)
	{
		return;
	}

	const float DamageMultiplier = UNovaParagonGruxBossComponent::GetIncomingDamageMultiplierForActor(DamagedActor);
	const float FinalDamage = Damage * DamageMultiplier;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - FinalDamage);
	RefreshHealthBar();

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
}

void UNovaFloatingHealthBarComponent::HandleDeath()
{
	ApplyHealthBarPercent(0.0f);

	if (!bDestroyOwnerOnDeath)
	{
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->SetLifeSpan(DeathDestroyDelay);
	}
}
