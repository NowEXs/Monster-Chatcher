// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"
#include "GameFramework/PlayerState.h"	// needed for GetPlayerId()
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UHealthComponent::UHealthComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// PlayerState isn't ready yet at BeginPlay; set SecretStat shortly after.
		FTimerHandle TH;
		GetWorld()->GetTimerManager().SetTimer(TH, this,
			&UHealthComponent::InitSecretStat, 0.5f, false);
	}
	
}

void UHealthComponent::InitSecretStat()
{
	if (APawn* P = Cast<APawn>(GetOwner()))
	{
		if (APlayerState* PS = P->GetPlayerState())
		{
			SecretStat = 100 + PS->GetPlayerId();   // distinct per player
		}
	}
}

// Called every frame
void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	// Leave it empty for now.
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
	
	DOREPLIFETIME_CONDITION(UHealthComponent, SecretStat, COND_OwnerOnly);
}

void UHealthComponent::OnRep_SecretStat()
{	
	FString Who = TEXT("unknown");
	if (APawn* P = Cast<APawn>(GetOwner()))
	{
		if (APlayerState* PS = P->GetPlayerState())
		{
			Who = FString::Printf(TEXT("PlayerId %d"), PS->GetPlayerId());
		}
	}
	UE_LOG(LogTemp, Warning,
		TEXT("OnRep on [%s] received SecretStat = %d"), *Who, SecretStat);
}

void UHealthComponent::ApplyDamage(float Damage)
{
	if (Damage < 0.0f) { return; }          // given: ignore negative damage
	
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	
	Health -= Damage; 
	
	if (Health < 0.0f) { Health = 0.0f; }
}

float UHealthComponent::GetHealthPercent() const
{
	if (MaxHealth <= 0.0f) { return 0.0f; } // given: guard divide-by-zero
	// BLANK: return the fraction of health remaining (0..1).
	// hint:  return Health / MaxHealth;
	/* TODO 3: one line -- copy the hint */
	return Health / MaxHealth;   // replace this line with the hint
}

bool UHealthComponent::IsDead() const
{
	// BLANK: dead when health has reached zero (or below).
	// hint:  return Health <= 0.0f;
	/* TODO 4: one line -- copy the hint */
	return Health <= 0.0f;  // replace this line with the hint
}

void UHealthComponent::Heal(float Amount)
{
	if (Amount < 0.0f) { return; }          // given: ignore negative heal
	Health += Amount;                        // given
	// BLANK: don't let health exceed MaxHealth.
	// hint:  if (Health > MaxHealth) { Health = MaxHealth; }
	/* TODO 2: one line -- copy the hint */
	if (Health > MaxHealth) { Health = MaxHealth; }
}
