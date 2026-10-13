// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class ARENAGAME_API AArenaPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	UPROPERTY(ReplicatedUsing=OnRep_Kills, BlueprintReadOnly)
	int32 Kills = 0;
	
	UFUNCTION()
	void OnRep_Kills();
	
	void AddKill();	// SV calls this
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
