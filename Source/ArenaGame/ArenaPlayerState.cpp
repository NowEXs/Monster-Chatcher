// Fill out your copyright notice in the Description page of Project Settings.


#include "ArenaPlayerState.h"
#include "Net/unrealNetwork.h"

void AArenaPlayerState::OnRep_Kills()
{
	UE_LOG(LogTemp, Log, TEXT("%s (Id %d) Kills now %d"), *GetPlayerName(), GetPlayerId(), Kills);
}

void AArenaPlayerState::AddKill()
{
	Kills += 1;
	OnRep_Kills();
}

void AArenaPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArenaPlayerState, Kills)
}
