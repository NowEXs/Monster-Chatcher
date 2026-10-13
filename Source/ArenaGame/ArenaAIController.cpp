// Fill out your copyright notice in the Description page of Project Settings.


#include "ArenaAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Team.h"	// WK 11
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Team.h"	// WK 11
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"	// WK11

// Sets default values
AArenaAIController::AArenaAIController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// This component must exist before any sense
	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AI Perception")));
	
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1200.0f;
	SightConfig->LoseSightRadius = 1500.0f;
	SightConfig->PeripheralVisionAngleDegrees = 60.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	GetPerceptionComponent()->ConfigureSense(*SightConfig);
	
	// Hearing - WK10
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	HearingConfig->HearingRange = 3000.f;
	HearingConfig->DetectionByAffiliation.bDetectEnemies    = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = false;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	HearingConfig->SetMaxAge(8.f);
	GetPerceptionComponent()->ConfigureSense(*HearingConfig);
	
	// Damage - WK10
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("Damage"));
	DamageConfig->SetMaxAge(8.f);
	GetPerceptionComponent()->ConfigureSense(*DamageConfig);
	
	// Team - WK 11
	TeamConfig = CreateDefaultSubobject<UAISenseConfig_Team>(TEXT("Team"));;
	TeamConfig->SetMaxAge(8.0f);
	GetPerceptionComponent()->ConfigureSense(*TeamConfig);
	
	GetPerceptionComponent()->SetDominantSense( SightConfig->GetSenseImplementation());
	
}

// Called when the game starts or when spawned
void AArenaAIController::BeginPlay()
{
	Super::BeginPlay();
	
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		const ETeamAttitude::Type Att = GetTeamAttitudeTowards(*PlayerPawn);
		UE_LOG(LogTemp, Warning, TEXT("AI TEAM: %d | ATTITUDE TO PLAYER: %s"),
		(int32)GetGenericTeamId().GetId(),
		Att == ETeamAttitude::Hostile  ? TEXT("Hostile")  :
		Att == ETeamAttitude::Friendly ? TEXT("Friendly") : TEXT("Neutral"));
	}
	
	if (UAIPerceptionComponent* Perception = GetPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddDynamic(this, &AArenaAIController::OnTargetSensed);
	}
	
}

void AArenaAIController::OnTargetSensed(AActor* Actor, FAIStimulus Stimulus)
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (BB == nullptr || Actor == nullptr) { return; }
	
	// WK 10
	// Ask the perception system WHICH sense produced this stimulus.
	const TSubclassOf<UAISense> Sense = UAIPerceptionSystem::GetSenseClassForStimulus(this, Stimulus);
	
	// TEMPORARY -- delete once team sense is confirmed
	UE_LOG(LogTemp, Warning, TEXT("[%s] SENSED: %s from %s, success=%d"),
		*GetNameSafe(GetPawn()), *GetNameSafe(Sense), *GetNameSafe(Actor),
		Stimulus.WasSuccessfullySensed() ? 1 : 0);
	
	if (Sense == UAISense_Sight::StaticClass())
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			BB->SetValueAsObject(TargetActorKey, Actor);
		}
		else
		{
			BB->ClearValue(TargetActorKey);
		}
		return;
	}

	if (Sense == UAISense_Damage::StaticClass())
	{
		// A hit is always news. There is no "stopped being hit".
		BB->SetValueAsVector(DamageLocationKey, Stimulus.StimulusLocation);
		return;
	}

	if (Sense == UAISense_Hearing::StaticClass())
	{
		BB->SetValueAsVector(InvestigateLocationKey, Stimulus.StimulusLocation);
	}
	
	// WK 11
	if (Sense == UAISense_Team::StaticClass())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] TEAM SHOUT -> investigate %s"),
		*GetNameSafe(GetPawn()), *Stimulus.StimulusLocation.ToString());
		BB->SetValueAsVector(InvestigateLocationKey, Stimulus.StimulusLocation);
	}
	
	// OLD from WK 9
	// if (Stimulus.WasSuccessfullySensed())
	// {
	// 	BB->SetValueAsObject(TargetActorKey, Actor);
	// }
	// else
	// {
	// 	BB->ClearValue(TargetActorKey);
	// }
}

void AArenaAIController::ReportTrouble(AActor* Enemy)
{
	if (GetPawn() == nullptr) { return; }

	FAITeamStimulusEvent TeamEvent(this, Enemy,
		GetPawn()->GetActorLocation(), 3000.f);
	UAIPerceptionSystem::OnEvent(GetWorld(), TeamEvent);
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] SHOUTING, enemy=%s"),
	*GetNameSafe(GetPawn()), *GetNameSafe(Enemy));
}

void AArenaAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}
}

FGenericTeamId AArenaAIController::GetGenericTeamId() const
{
	// AI on team 1
	return FGenericTeamId(1);
}

