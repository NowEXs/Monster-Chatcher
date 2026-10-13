// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "ArenaGameGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AArenaGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AArenaGameGameMode();
	
	UPROPERTY(EditDefaultsOnly, Category="Rules")
	int32 WinningScore = 3;
	
	void OnCharacterKilled(AController* Killer, AController* Victim);
	void CheckWinner(class AArenaPlayerState* Scorer);
	
	UFUNCTION(BlueprintCallable, Category="Rules")
	void TravelToArena();
	
	UPROPERTY(EditDefaultsOnly, category="AI")
	TSubclassOf<APawn> AICharacterClass;	// set to BP_ArenaAI
	
	UFUNCTION(BlueprintCallable, Category="AI")
	void SpawnAICharacter(const FTransform& SpawnXform);
	
	// WK12
	UFUNCTION(BlueprintCallable, Category="Wave")
	void StartWave(const TArray<AActor*> & Points);
	
	UPROPERTY(EditDefaultsOnly, Category="Wave")
	TArray<int32> WaveSize = { 2, 4, 6 };
	
	UPROPERTY(EditDefaultsOnly, Category="Wave")
	float SpawnInterval = 3.0f;

protected:
	// WK12
	TArray<AActor*> SpawnPoints;
	FTransform PickSpawnTransform() const;
	
	void SpawnWave();
	bool AnyEnemiesLeft() const;
	
	FTimerHandle WaveTimerHandle;
	int32 CurrentWave = 0;
	int32 SpawnedSoFar = 0;
	
	void StopAllAI();
};



