// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_MeleeAttack.h"
#include "AIController.h"
#include "ArenaGameCharacter.h"

UBTTask_MeleeAttack::UBTTask_MeleeAttack()
{
	NodeName = TEXT("Melee Attack");
}

EBTNodeResult::Type UBTTask_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	
	if (AICon == nullptr) { return  EBTNodeResult::Failed; }
	
	AArenaGameCharacter* Me = Cast<AArenaGameCharacter>(AICon->GetPawn());
	
	if (Me == nullptr) { return EBTNodeResult::Failed; }
	
	Me->ServerMeleeAttack();
	
	return EBTNodeResult::Succeeded;
}
