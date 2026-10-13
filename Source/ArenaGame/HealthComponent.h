// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARENAGAME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHealthComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	void InitSecretStat();	 // sets SecretStat after PlayerState is ready

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.0f;
	
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Health")
	float Health = 100.0f;
	
	UFUNCTION()
	void OnRep_Health(float OldHealth);
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(ReplicatedUsing = OnRep_SecretStat)
	int32 SecretStat = 0;	// private info for specific player only
	
	UFUNCTION()
	void OnRep_SecretStat();	// runs on the owning client when the value arrives
	
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Damage);
	
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);
	
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const;
	
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const;
		
};
