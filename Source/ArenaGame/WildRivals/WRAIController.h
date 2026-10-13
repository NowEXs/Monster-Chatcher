#pragma once
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "WRAIController.generated.h"
class UBehaviorTree;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;

UCLASS()
class ARENAGAME_API AWRAIController : public AAIController
{
    GENERATED_BODY()
public:
    AWRAIController();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wild Rivals|AI") TObjectPtr<UBehaviorTree> BehaviorTreeAsset;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wild Rivals|AI") TObjectPtr<UAISenseConfig_Sight> SightConfig;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wild Rivals|AI") TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wild Rivals|AI") TObjectPtr<UAISenseConfig_Damage> DamageConfig;
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void ResetAfterTeamChange();
    void ClearInvestigation();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Wild Rivals|AI") void RefreshDecision();
private:
    FTimerHandle DecisionTimer;
    FVector HeardLocation = FVector::ZeroVector;
    FVector DamageLocation = FVector::ZeroVector;
    float HeardUntil = 0.f;
    float DamageUntil = 0.f;
    bool bReturningToOwner = false;
    bool IsThreat(const AActor* Actor) const;
    UFUNCTION() void OnSensed(AActor* Actor, FAIStimulus Stimulus);
};
