// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "ArenaAIController.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class UAISenseConfig_Team;	// WK 11

UCLASS()
class ARENAGAME_API AArenaAIController : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArenaAIController();
	virtual void OnPossess(APawn* InPawn) override;
	
	// AI on Team 1
	virtual FGenericTeamId GetGenericTeamId() const override;
	
	// WK 11
	UFUNCTION()
	void ReportTrouble(AActor* Enemy);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnTargetSensed(AActor* Actor, FAIStimulus Stimulus);
	
	// Assigned in the Blueprint child, BP_ArenaAIController.
	UPROPERTY(EditAnywhere, Category="AI")
	UBehaviorTree* BehaviorTreeAsset;
	
	UPROPERTY(VisibleAnywhere, Category="AI")
	UAISenseConfig_Sight* SightConfig;
	
	UPROPERTY(VisibleAnywhere, Category="AI")
	UAISenseConfig_Hearing* HearingConfig;  // new, lab 10

	UPROPERTY(VisibleAnywhere, Category="AI")
	UAISenseConfig_Damage* DamageConfig;    // new, lab 10
	
	UPROPERTY(VisibleAnywhere, Category="AI")
	UAISenseConfig_Team* TeamConfig;	// new, WK 11
	
	UPROPERTY(EditDefaultsOnly, Category="AI")
	FName TargetActorKey = TEXT("TargetActor");
	
	// WK 10
	UPROPERTY(EditDefaultsOnly, Category="AI")
	FName DamageLocationKey = TEXT("LastDamageLocation");
	
	UPROPERTY(EditDefaultsOnly, Category="AI")
	FName InvestigateLocationKey = TEXT("InvestigateLocation");
	
};
