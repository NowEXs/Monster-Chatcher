#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "WRBTTasks.generated.h"

UCLASS()
class ARENAGAME_API UWRBTTask_PickMovePoint : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UWRBTTask_PickMovePoint();
    UPROPERTY(EditAnywhere, Category="Wild Rivals") bool bFlee = false;
    UPROPERTY(EditAnywhere, Category="Wild Rivals", meta=(ClampMin="50")) float Radius = 650.f;
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
UCLASS()
class ARENAGAME_API UWRBTTask_Attack : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UWRBTTask_Attack();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
UCLASS()
class ARENAGAME_API UWRBTTask_ClearInvestigation : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UWRBTTask_ClearInvestigation();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
