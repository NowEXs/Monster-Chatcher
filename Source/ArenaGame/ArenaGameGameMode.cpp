// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaGameGameMode.h"
#include "EngineUtils.h"
#include "BrainComponent.h"
#include "ArenaAIController.h"
#include "ArenaPlayerState.h"
#include "ArenaGameState.h"

AArenaGameGameMode::AArenaGameGameMode()
{
	// stub
}

void AArenaGameGameMode::OnCharacterKilled(AController* Killer, AController* Victim)
{
	// Kill credit. Both checks matter:
	//  - no Killer: an environment kill; nobody scores.
	//  - no PlayerState: a non-player controller has none by
	//    default, so such kills silently score nothing.
	if (Killer)
	{
		if (AArenaPlayerState* PS =
		Killer->GetPlayerState<AArenaPlayerState>())
		{
			PS->AddKill();
			CheckWinner(PS);
		}
	}
		
	// Clear the body, then respawn ONLY players.
	if (Victim)
	{
		if (APawn* DeadPawn = Victim->GetPawn())
		{
			DeadPawn->Destroy();
		}
			
		// Do not respawn into a finished round.
		const AArenaGameState* GS = GetGameState<AArenaGameState>();
		const bool bRoundStillRunning = (GS == nullptr) || !GS->bRoundOver;
		
		// BLANK 3: respawn the victim only if it is a player.
		// hint: bRoundStillRunning && Victim->IsPlayerController()
		if (bRoundStillRunning && Victim->IsPlayerController())
		{
			if (AArenaGameState* MGS = GetGameState<AArenaGameState>())
			{
				MGS->WinnerId = -1;
				MGS->bRoundOver = true;
				MGS->OnRep_RoundOver();
			}
			// RestartPlayer(Victim);
		}
	}
}

void AArenaGameGameMode::CheckWinner(class AArenaPlayerState* Scorer)
{
	// BLANK 4: has this player reached the winning score?
	// hint: Scorer->Kills >= WinningScore
	if (Scorer->Kills >= WinningScore)
	{
		if (AArenaGameState* GS = GetGameState<AArenaGameState>())
		{
			GS->WinnerId   = Scorer->GetPlayerId();
			GS->bRoundOver = true;
			GS->OnRep_RoundOver();  // manual on server (Wk5 rule)
		}
	}
}

void AArenaGameGameMode::TravelToArena()
{
	// SERVER ONLY -- every connected client follows automatically.
	GetWorld()->ServerTravel(TEXT("/Game/Maps/Arena?listen"));
}

void AArenaGameGameMode::SpawnAICharacter(const FTransform& SpawnXform)
{
	if (!AICharacterClass)
	{
		return;
	}
	
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;	// do not save into the map
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		
	GetWorld()->SpawnActor<APawn>(AICharacterClass, SpawnXform, Params);
}

// WK12
void AArenaGameGameMode::StartWave(const TArray<AActor*>& Points)
{
	SpawnPoints = Points;
	CurrentWave = 0;
	SpawnedSoFar = 0;
	
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AArenaGameGameMode::SpawnWave, SpawnInterval, true);
}

FTransform AArenaGameGameMode::PickSpawnTransform() const
{
	if (SpawnPoints.Num() == 0)
	{
		return FTransform::Identity;
	}
	
	const int32 Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
	
	return SpawnPoints[Index]->GetActorTransform();
}

void AArenaGameGameMode::SpawnWave()
{
	const AArenaGameState* GS = GetGameState<AArenaGameState>();
	
	if (GS && GS->bRoundOver)
	{
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);
		StopAllAI();
		return;
	}
	
	// if (SpawnedSoFar >= WaveQuota) { return; }
	
	const int32 ThisWaveSize = WaveSize[CurrentWave];
	if (SpawnedSoFar >= ThisWaveSize)
	{
		if (AnyEnemiesLeft()) { return; }
		
		CurrentWave++;
		SpawnedSoFar = 0;
		
		if (CurrentWave >= WaveSize.Num())
		{
			UE_LOG(LogTemp, Warning, TEXT("ALL WAVE CLEARED"));
			
			if (AArenaGameState* GS2 = GetGameState<AArenaGameState>())
			{
				GS2->WinnerId = 0;
				GS2->bRoundOver = true;
				GS2->OnRep_RoundOver();
			}
		} 
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("WAVE %d BEGINS -- %d enemies"), CurrentWave+1, WaveSize[CurrentWave]);
		}
		return;
	}
	
	SpawnAICharacter(PickSpawnTransform());
	SpawnedSoFar++;
	UE_LOG(LogTemp, Warning, TEXT("WAVE: %d: sent %d of %d"), CurrentWave+1, SpawnedSoFar, ThisWaveSize);
}

bool AArenaGameGameMode::AnyEnemiesLeft() const
{
	for (TActorIterator<AArenaAIController> It(GetWorld()); It; ++It)
	{
		if (It->GetPawn() != nullptr) { return true; }
	}
	return false;
}

void AArenaGameGameMode::StopAllAI()
{
	for (TActorIterator<AArenaAIController> It(GetWorld()); It; ++It)
	{
		if (UBrainComponent* Brain = It->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Round over"));
		}
	}
}
