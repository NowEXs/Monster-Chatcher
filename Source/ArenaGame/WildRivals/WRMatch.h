#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGamePlayerController.h"
#include "WRMatch.generated.h"

class AWRCreature;
class AWRCharacter;

UENUM(BlueprintType)
enum class EWRRoundPhase : uint8 { Waiting, Playing, Finished };

USTRUCT(BlueprintType)
struct FWRRoundState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EWRRoundPhase Phase = EWRRoundPhase::Waiting;
    UPROPERTY(BlueprintReadOnly) int32 Wave = 0;
    UPROPERTY(BlueprintReadOnly) int32 TargetCaptures = 5;
    // 0 = P1, 1 = P2, -1 = draw/no winner yet. Do NOT use PlayerId as team.
    UPROPERTY(BlueprintReadOnly) int32 WinnerTeam = -1;
    UPROPERTY(BlueprintReadOnly) float EndsAtServerTime = 0.f;
    UPROPERTY(BlueprintReadOnly) FString ResultReason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWRStateChanged);

UCLASS()
class ARENAGAME_API AWRPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    UPROPERTY(ReplicatedUsing=OnRep_PublicState, BlueprintReadOnly, Category="Wild Rivals") uint8 TeamId = 255;
    UPROPERTY(ReplicatedUsing=OnRep_PublicState, BlueprintReadOnly, Category="Wild Rivals") int32 Captures = 0;
    // PlayerState survives pawn death; a pet follows the owner's NEW pawn after respawn.
    UPROPERTY(ReplicatedUsing=OnRep_PublicState, BlueprintReadOnly, Category="Wild Rivals") TObjectPtr<AWRCreature> Companion;
    UPROPERTY(BlueprintAssignable, Category="Wild Rivals") FWRStateChanged OnPublicStateChanged;
    UFUNCTION() void OnRep_PublicState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class ARENAGAME_API AWRGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(ReplicatedUsing=OnRep_Round, BlueprintReadOnly, Category="Wild Rivals") FWRRoundState Round;
    UPROPERTY(BlueprintAssignable, Category="Wild Rivals") FWRStateChanged OnRoundChanged;
    UFUNCTION() void OnRep_Round();
    UFUNCTION(BlueprintPure, Category="Wild Rivals") float GetRemainingSeconds() const;
    UFUNCTION(BlueprintPure, Category="Wild Rivals") AWRPlayerState* GetPlayerForTeam(int32 Team) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Audio")
    TObjectPtr<USoundBase> BeginRoundSound;

    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Audio")
    TObjectPtr<USoundBase> VictorySound;
    
private:
    EWRRoundPhase PreviousPhase = EWRRoundPhase::Waiting;
};

UCLASS()
class ARENAGAME_API AWRPlayerController : public AArenaGamePlayerController
{
    GENERATED_BODY()
public:
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};

UCLASS()
class ARENAGAME_API AWRGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AWRGameMode();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wild Rivals|Rules", meta=(ClampMin="1")) int32 CaptureGoal = 5;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Rules", meta=(ClampMin="1", ClampMax="2")) int32 RequiredPlayers = 2;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Rules", meta=(ClampMin="10")) float RoundDuration = 180.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Rules", meta=(ClampMin="1")) float RespawnDelay = 3.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Rules") bool bAllowPlayerVsPlayerMelee = false;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves") TSubclassOf<AWRCreature> ShyCreatureClass;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves") TSubclassOf<AWRCreature> AggressiveCreatureClass;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="1")) int32 TotalWaves = 3;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="1")) float WaveInterval = 45.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="0")) int32 ShyPerWave = 4;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="0")) int32 AggressivePerWave = 2;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="0")) int32 MaxWildAggressive = 2;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="1")) int32 MaxActiveShy = 12;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Waves", meta=(ClampMin="0")) float SpawnScatterRadius = 160.f;
    UFUNCTION(BlueprintPure, Category="Wild Rivals") bool IsRoundPlaying() const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Wild Rivals") void StartRound();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Wild Rivals") void RestartRound();
    void AwardCapture(AWRPlayerState* Scorer);
    void HandlePlayerDefeated(AWRCharacter* Victim);
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    FTimerHandle MatchTimer;
    float StartedAt = 0.f;
    float ReadySince = -1.f;
    int32 PendingShy = 0;
    int32 PendingAggressive = 0;
    int32 ShyCursor = 0;
    int32 AggressiveCursor = 0;
    bool bLoggedSetupError = false;
    void UpdateMatch();
    void BeginNextWave();
    void SpawnPending();
    bool SpawnOne(bool bShy);
    int32 CountCreatures(bool bShy) const;
    bool HaveReadyPlayers() const;
    void FinishRound(int32 WinnerTeam, const FString& Reason);
};
