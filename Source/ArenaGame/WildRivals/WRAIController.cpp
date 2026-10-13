#include "WRAIController.h"
#include "WRCharacter.h"
#include "WRMatch.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "TimerManager.h"
#include "Engine/World.h"

AWRAIController::AWRAIController()
{
    SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("WRPerception")));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("WRSight"));
    SightConfig->SightRadius = 1100.f;
    SightConfig->LoseSightRadius = 1400.f;
    SightConfig->PeripheralVisionAngleDegrees = 75.f;
    SightConfig->SetMaxAge(3.f);
    // Sense all affiliations, then filter using live pawn state. Dynamic taming must not
    // depend on stale engine perception affiliation caches or a global attitude solver.
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("WRHearing"));
    HearingConfig->HearingRange = 1800.f;
    HearingConfig->SetMaxAge(5.f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("WRDamage"));
    DamageConfig->SetMaxAge(4.f);
    GetPerceptionComponent()->ConfigureSense(*SightConfig);
    GetPerceptionComponent()->ConfigureSense(*HearingConfig);
    GetPerceptionComponent()->ConfigureSense(*DamageConfig);
    GetPerceptionComponent()->SetDominantSense(SightConfig->GetSenseImplementation());
}
FGenericTeamId AWRAIController::GetGenericTeamId() const
{
    const AWRCharacter* Body = Cast<AWRCharacter>(GetPawn());
    return Body ? Body->GetGenericTeamId() : FGenericTeamId::NoTeam;
}
ETeamAttitude::Type AWRAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
    const AWRCharacter* Body = Cast<AWRCharacter>(GetPawn());
    const AWRCharacter* Target = Cast<AWRCharacter>(&Other);
    if (!Body || !Target) return ETeamAttitude::Neutral;
    if (Body->TeamId == Target->TeamId) return ETeamAttitude::Friendly;
    return IsThreat(&Other) ? ETeamAttitude::Hostile : ETeamAttitude::Neutral;
}
bool AWRAIController::IsThreat(const AActor* Actor) const
{
    const AWRCreature* Me = Cast<AWRCreature>(GetPawn());
    const AWRCharacter* Other = Cast<AWRCharacter>(Actor);
    if (!Me || !Other || Other == Me || !Other->IsAlive()) return false;
    if (Me->Kind == EWRCreatureKind::Shy) return Other->IsPlayerControlled();
    return Me->IsEnemy(Other);
}
void AWRAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (!HasAuthority()) return;
    GetPerceptionComponent()->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AWRAIController::OnSensed);
    if (!BehaviorTreeAsset || !RunBehaviorTree(BehaviorTreeAsset))
    {
        UE_LOG(LogTemp, Error, TEXT("WR: %s has no working BehaviorTreeAsset. Assign BT_WR_Creature in BP_WR_AIController."), *GetName());
        return;
    }
    const TCHAR* RequiredKeys[] = { TEXT("RoundActive"), TEXT("ShouldFlee"), TEXT("ThreatLocation"),
        TEXT("TargetActor"), TEXT("InvestigateLocation"), TEXT("FollowActor"), TEXT("ShouldFollow"),
        TEXT("IsTamed"), TEXT("MoveTarget") };
    for (const TCHAR* Key : RequiredKeys)
    {
        if (!GetBlackboardComponent() || GetBlackboardComponent()->GetKeyID(FName(Key)) == FBlackboard::InvalidKey)
        {
            UE_LOG(LogTemp, Error, TEXT("WR Blackboard missing key: %s"), Key);
            if (GetBrainComponent()) GetBrainComponent()->StopLogic(TEXT("WR Blackboard setup incomplete"));
            return;
        }
    }
    GetWorldTimerManager().SetTimer(DecisionTimer, this, &AWRAIController::RefreshDecision, 0.2f, true);
}
void AWRAIController::OnUnPossess()
{
    GetWorldTimerManager().ClearTimer(DecisionTimer);
    GetPerceptionComponent()->OnTargetPerceptionUpdated.RemoveDynamic(this, &AWRAIController::OnSensed);
    if (GetBrainComponent()) GetBrainComponent()->StopLogic(TEXT("Unpossessed"));
    Super::OnUnPossess();
}
void AWRAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(DecisionTimer);
    Super::EndPlay(Reason);
}
void AWRAIController::ClearInvestigation()
{
    HeardUntil = DamageUntil = 0.f;
    if (UBlackboardComponent* BB = GetBlackboardComponent()) BB->ClearValue(TEXT("InvestigateLocation"));
}
void AWRAIController::ResetAfterTeamChange()
{
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    ClearInvestigation();
    bReturningToOwner = false;
    if (UBlackboardComponent* BB = GetBlackboardComponent())
    {
        BB->ClearValue(TEXT("TargetActor"));
        BB->ClearValue(TEXT("ThreatLocation"));
        BB->SetValueAsBool(TEXT("ShouldFlee"), false);
    }
    GetPerceptionComponent()->ForgetAll();
    GetPerceptionComponent()->RequestStimuliListenerUpdate();
    RefreshDecision();
}
void AWRAIController::OnSensed(AActor* Actor, FAIStimulus Stimulus)
{
    if (!HasAuthority() || !IsThreat(Actor)) return;
    const float Now = GetWorld()->GetTimeSeconds();
    const TSubclassOf<UAISense> Sense = UAIPerceptionSystem::GetSenseClassForStimulus(this, Stimulus);
    if (Sense == UAISense_Sight::StaticClass())
    {
        if (!Stimulus.WasSuccessfullySensed())
        {
            // Keep the reported LAST SEEN point, never read a hidden actor's new position.
            if (UBlackboardComponent* BB = GetBlackboardComponent())
                if (BB->GetValueAsObject(TEXT("TargetActor")) == Actor)
                { HeardLocation = Stimulus.StimulusLocation; HeardUntil = Now + 3.f; }
        }
    }
    else if (Sense == UAISense_Damage::StaticClass() && Stimulus.WasSuccessfullySensed())
    { DamageLocation = Stimulus.StimulusLocation; DamageUntil = Now + 4.f; }
    else if (Sense == UAISense_Hearing::StaticClass() && Stimulus.WasSuccessfullySensed())
    { HeardLocation = Stimulus.StimulusLocation; HeardUntil = Now + 5.f; }
    // Periodic refresh reads perception after it has committed the current update.
}
void AWRAIController::RefreshDecision()
{
    AWRCreature* Me = Cast<AWRCreature>(GetPawn());
    UBlackboardComponent* BB = GetBlackboardComponent();
    const AWRGameState* GS = GetWorld()->GetGameState<AWRGameState>();
    if (!Me || !BB || !Me->IsAlive()) return;
    const bool bPlaying = GS && GS->Round.Phase == EWRRoundPhase::Playing;
    BB->SetValueAsBool(TEXT("RoundActive"), bPlaying);
    if (!bPlaying) { StopMovement(); return; }
    AWRCharacter* OwnerCharacter = Me->GetOwnerCharacter();
    BB->SetValueAsObject(
    TEXT("FollowActor"),
    OwnerCharacter && OwnerCharacter->IsAlive() ? OwnerCharacter : nullptr
);
    BB->SetValueAsBool(TEXT("IsTamed"), Me->CreatureState == EWRCreatureState::Tamed);
    BB->SetValueAsBool(TEXT("ShouldFollow"), false);
    BB->SetValueAsBool(TEXT("ShouldFlee"), false);
    if (Me->CreatureState == EWRCreatureState::Tamed)
    {
        // Wait during owner's respawn; do not continue hunting without an owner pawn.
        if (!OwnerCharacter || !OwnerCharacter->IsAlive())
        {
            BB->ClearValue(TEXT("TargetActor"));
            ClearInvestigation();
            ClearFocus(EAIFocusPriority::Gameplay);
            return;
        }
        const float Distance = Me->GetDistanceTo(OwnerCharacter);
        if (Distance > Me->LeashDistance) bReturningToOwner = true;
        if (Distance <= Me->FollowDistance) bReturningToOwner = false;
        if (bReturningToOwner)
        {
            BB->ClearValue(TEXT("TargetActor"));
            ClearInvestigation();
            BB->SetValueAsBool(TEXT("ShouldFollow"), true);
            ClearFocus(EAIFocusPriority::Gameplay);
            return;
        }
    }
    TArray<AActor*> Seen;
    GetPerceptionComponent()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);
    AActor* Best = nullptr;
    float BestDistance = TNumericLimits<float>::Max();
    for (AActor* Actor : Seen)
    {
        if (!IsThreat(Actor)) continue;
        if (OwnerCharacter &&
    FVector::DistSquared(
        OwnerCharacter->GetActorLocation(),
        Actor->GetActorLocation()
    ) > FMath::Square(Me->LeashDistance))
        {
            continue;
        }
        const float Distance = FVector::DistSquared(Me->GetActorLocation(), Actor->GetActorLocation());
        if (Distance < BestDistance) { Best = Actor; BestDistance = Distance; }
    }
    const float Now = GetWorld()->GetTimeSeconds();
    const bool bHaveMemory = DamageUntil > Now || HeardUntil > Now;
    const FVector MemoryLocation = DamageUntil > Now ? DamageLocation : HeardLocation;
    if (Me->Kind == EWRCreatureKind::Shy)
    {
        BB->ClearValue(TEXT("TargetActor"));
        BB->ClearValue(TEXT("InvestigateLocation"));
        ClearFocus(EAIFocusPriority::Gameplay);
        if (Best || bHaveMemory)
        {
            BB->SetValueAsVector(TEXT("ThreatLocation"), Best ? Best->GetActorLocation() : MemoryLocation);
            BB->SetValueAsBool(TEXT("ShouldFlee"), true);
        }
        return;
    }
    BB->SetValueAsObject(TEXT("TargetActor"), Best);
    if (Best)
    {
        SetFocus(Best);
        // A sighting supersedes old sounds/damage so an old hostile report cannot
        // send an AI back toward its new owner after a team change.
        ClearInvestigation();
    }
    else
    {
        ClearFocus(EAIFocusPriority::Gameplay);
        if (bHaveMemory) BB->SetValueAsVector(TEXT("InvestigateLocation"), MemoryLocation);
        else BB->ClearValue(TEXT("InvestigateLocation"));
        if (!bHaveMemory &&
    OwnerCharacter &&
    OwnerCharacter->IsAlive() &&
    Me->GetDistanceTo(OwnerCharacter) > Me->FollowDistance)
        {
            BB->SetValueAsBool(TEXT("ShouldFollow"), true);
        }
    }
}
