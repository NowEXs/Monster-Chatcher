// Fill out your copyright notice in the Description page of Project Settings.


#include "ArenaGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"                    // <- ACharacter
#include "GameFramework/CharacterMovementComponent.h"   // <- StopMovementImmediately

void AArenaGameState::OnRep_RoundOver()
{
	if (!bRoundOver) {return;}
	
	UE_LOG(LogTemp, Log, TEXT("ROUND OVER -- winner PlyerID %d"), WinnerId);
	
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);

		if (APawn* P = PC->GetPawn())
		{
			P->DisableInput(PC);
			if (ACharacter* C = Cast<ACharacter>(P))
			{
				C->GetCharacterMovement()->StopMovementImmediately();
			}
		}
	}
	
}

void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AArenaGameState, bRoundOver);
	DOREPLIFETIME(AArenaGameState, WinnerId);
}
