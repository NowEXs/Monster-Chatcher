// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaGameState.generated.h"

/**
 * 
 */
UCLASS()
class ARENAGAME_API AArenaGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(ReplicatedUsing=OnRep_RoundOver, BlueprintReadOnly)
	bool bRoundOver;
	
	UPROPERTY(Replicated, blueprintReadOnly)
	int32 WinnerId = -1;
	
	UFUNCTION()
	void OnRep_RoundOver();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
