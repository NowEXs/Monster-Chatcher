#include "WRBTTasks.h"
#include "WRCharacter.h"
#include "WRAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"

UWRBTTask_PickMovePoint::UWRBTTask_PickMovePoint() { NodeName = TEXT("WR Pick Move Point"); }
EBTNodeResult::Type UWRBTTask_PickMovePoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    AAIController* AI = OwnerComp.GetAIOwner();
    APawn* Pawn = AI ? AI->GetPawn() : nullptr;
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
    if (!Pawn || !BB || !Nav) return EBTNodeResult::Failed;
    const FVector Origin = Pawn->GetActorLocation();
    const FVector Threat = BB->GetValueAsVector(TEXT("ThreatLocation"));
    FNavLocation Best;
    bool bFound = false;
    // Reachable samples avoid selecting the far side of a wall. Flee must make
    // positive progress away from the stimulus; failure falls through to Wait.
    float BestScore = bFlee ? FVector::DistSquared2D(Origin, Threat) + 100.f : -1.f;
    if (bFlee)
    {
        AWRCreature* WR_Pawn = Cast<AWRCreature>(Pawn);
        WR_Pawn->ChangeToFleeSpeed();
    }
    else
    {
        AWRCreature* WR_Pawn = Cast<AWRCreature>(Pawn);
        WR_Pawn->ChangeToShySpeed();
    }
    for (int32 I = 0; I < (bFlee ? 16 : 4); ++I)
    {
        FNavLocation Candidate;
        if (!Nav->GetRandomReachablePointInRadius(Origin, Radius, Candidate)) continue;
        const float Score = bFlee ? FVector::DistSquared2D(Candidate.Location, Threat) : 1.f;
        if (Score > BestScore) { BestScore = Score; Best = Candidate; bFound = true; }
        if (bFound && !bFlee) break;
    }
    if (!bFound) return EBTNodeResult::Failed;
    BB->SetValueAsVector(TEXT("MoveTarget"), Best.Location);
    return EBTNodeResult::Succeeded;
}
UWRBTTask_Attack::UWRBTTask_Attack() { NodeName = TEXT("WR Attack"); }
EBTNodeResult::Type UWRBTTask_Attack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    const AAIController* AI = OwnerComp.GetAIOwner();
    AWRCharacter* Me = AI ? Cast<AWRCharacter>(AI->GetPawn()) : nullptr;
    const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TEXT("TargetActor"))) : nullptr;
    if (!Me || !Me->HasAuthority() || !Me->IsEnemy(Target) || Me->GetDistanceTo(Target) > Me->MeleeRange || !Me->HasClearPathTo(Target)) return EBTNodeResult::Failed;
    const FVector To = Target->GetActorLocation() - Me->GetActorLocation();
    Me->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
    Me->ServerMeleeAttack();
    return EBTNodeResult::Succeeded;
}
UWRBTTask_ClearInvestigation::UWRBTTask_ClearInvestigation() { NodeName = TEXT("WR Clear Investigation"); }
EBTNodeResult::Type UWRBTTask_ClearInvestigation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    if (AWRAIController* AI = Cast<AWRAIController>(OwnerComp.GetAIOwner()))
    { AI->ClearInvestigation(); return EBTNodeResult::Succeeded; }
    return EBTNodeResult::Failed;
}
