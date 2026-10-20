#include "WRMatch.h"
#include "WRRules.h"
#include "WRCharacter.h"
#include "HealthComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void AWRPlayerState::OnRep_PublicState() { OnPublicStateChanged.Broadcast(); }
void AWRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWRPlayerState, TeamId);
    DOREPLIFETIME(AWRPlayerState, Captures);
    DOREPLIFETIME(AWRPlayerState, Companion);
}
void AWRGameState::OnRep_Round()
{
    if (Round.Phase == EWRRoundPhase::Playing && PreviousPhase != EWRRoundPhase::Playing)
    {
        if (BeginRoundSound)
        {
            UGameplayStatics::PlaySound2D(this, BeginRoundSound);
        }
    }
    else if (Round.Phase == EWRRoundPhase::Finished && PreviousPhase != EWRRoundPhase::Finished)
    {
        if (VictorySound)
        {
            UGameplayStatics::PlaySound2D(this, VictorySound);
        }
    }
    
    PreviousPhase = Round.Phase;
    OnRoundChanged.Broadcast();
}
float AWRGameState::GetRemainingSeconds() const
{
    return Round.Phase == EWRRoundPhase::Playing ? FMath::Max(0.f, Round.EndsAtServerTime - GetServerWorldTimeSeconds()) : 0.f;
}
AWRPlayerState* AWRGameState::GetPlayerForTeam(int32 Team) const
{
    for (APlayerState* P : PlayerArray)
        if (AWRPlayerState* PS = Cast<AWRPlayerState>(P)) if (PS->TeamId == Team) return PS;
    return nullptr;
}
void AWRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWRGameState, Round);
}
FGenericTeamId AWRPlayerController::GetGenericTeamId() const
{
    const AWRPlayerState* PS = GetPlayerState<AWRPlayerState>();
    return PS ? FGenericTeamId(PS->TeamId) : FGenericTeamId::NoTeam;
}
ETeamAttitude::Type AWRPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
    const AWRCharacter* Me = Cast<AWRCharacter>(GetPawn());
    const AWRCharacter* Target = Cast<AWRCharacter>(&Other);
    if (!Me || !Target) return ETeamAttitude::Neutral;
    if (Me->TeamId == Target->TeamId) return ETeamAttitude::Friendly;
    return Me->IsEnemy(Target) ? ETeamAttitude::Hostile : ETeamAttitude::Neutral;
}

AWRGameMode::AWRGameMode()
{
    GameStateClass = AWRGameState::StaticClass();
    PlayerStateClass = AWRPlayerState::StaticClass();
    PlayerControllerClass = AWRPlayerController::StaticClass();
    DefaultPawnClass = AWRCharacter::StaticClass();
}
void AWRGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (AWRGameState* GS = GetGameState<AWRGameState>())
    { GS->Round.TargetCaptures = FMath::Max(1, CaptureGoal); GS->OnRep_Round(); }
    GetWorldTimerManager().SetTimer(MatchTimer, this, &AWRGameMode::UpdateMatch, 0.5f, true);
}
void AWRGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(Reason);
}
void AWRGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    AWRPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AWRPlayerState>() : nullptr;
    if (!PS) return;
    bool bUsed[2] = {false, false};
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || PC == NewPlayer) continue;
        const AWRPlayerState* Existing = PC->GetPlayerState<AWRPlayerState>();
        if (Existing && Existing->TeamId < 2) bUsed[Existing->TeamId] = true;
    }
    if (bUsed[0] && bUsed[1])
    {
        NewPlayer->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Wild Rivals supports two players.")));
        NewPlayer->Destroy();
        return;
    }
    PS->TeamId = bUsed[0] ? 1 : 0;
    PS->OnRep_PublicState();
    PS->ForceNetUpdate();
    if (AWRCharacter* Pawn = Cast<AWRCharacter>(NewPlayer->GetPawn()))
    { Pawn->TeamId = PS->TeamId; Pawn->OnRep_Team(); Pawn->ForceNetUpdate(); }
}
void AWRGameMode::Logout(AController* Exiting)
{
    AWRPlayerState* PS = Exiting ? Exiting->GetPlayerState<AWRPlayerState>() : nullptr;
    if (PS && IsValid(PS->Companion)) PS->Companion->RemoveFromMatch();
    if (PS && IsRoundPlaying()) FinishRound(PS->TeamId == 0 ? 1 : 0, TEXT("Opponent disconnected"));
    Super::Logout(Exiting);
}
AActor* AWRGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
    if (PS && PS->TeamId < 2)
    {
        const FName Desired = PS->TeamId == 0 ? TEXT("P1") : TEXT("P2");
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
            if (It->PlayerStartTag == Desired) return *It;
    }
    return Super::ChoosePlayerStart_Implementation(Player);
}
bool AWRGameMode::HaveReadyPlayers() const
{
    int32 Count = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        const AWRPlayerState* PS = PC ? PC->GetPlayerState<AWRPlayerState>() : nullptr;
        const AWRCharacter* Pawn = PC ? Cast<AWRCharacter>(PC->GetPawn()) : nullptr;
        if (PS && PS->TeamId < 2 && Pawn && Pawn->IsAlive()) ++Count;
    }
    return Count >= FMath::Clamp(RequiredPlayers, 1, 2);
}
bool AWRGameMode::IsRoundPlaying() const
{
    const AWRGameState* GS = GetGameState<AWRGameState>();
    return GS && GS->Round.Phase == EWRRoundPhase::Playing;
}
void AWRGameMode::StartRound()
{
    AWRGameState* GS = GetGameState<AWRGameState>();
    if (!HasAuthority() || !GS || GS->Round.Phase != EWRRoundPhase::Waiting || !HaveReadyPlayers()) return;
    TArray<AActor*> ShyPoints, AggressivePoints;
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("WR_Shy"), ShyPoints);
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("WR_Aggressive"), AggressivePoints);
    if (!ShyCreatureClass || !AggressiveCreatureClass || ShyPoints.IsEmpty() || AggressivePoints.IsEmpty())
    {
        if (!bLoggedSetupError)
            UE_LOG(LogTemp, Error, TEXT("WR setup: assign both creature classes and TargetPoints with Actor Tags WR_Shy / WR_Aggressive. Match remains Waiting."));
        bLoggedSetupError = true;
        return;
    }
    StartedAt = GetWorld()->GetTimeSeconds();
    GS->Round.Phase = EWRRoundPhase::Playing;
    GS->Round.TargetCaptures = FMath::Max(1, CaptureGoal);
    GS->Round.EndsAtServerTime = GS->GetServerWorldTimeSeconds() + FMath::Max(10.f, RoundDuration);
    GS->Round.Wave = 0;
    GS->Round.WinnerTeam = -1;
    GS->Round.ResultReason.Empty();
    GS->OnRep_Round();
    GS->ForceNetUpdate();
    BeginNextWave();
}
void AWRGameMode::UpdateMatch()
{
    AWRGameState* GS = GetGameState<AWRGameState>();
    if (!GS) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (GS->Round.Phase == EWRRoundPhase::Waiting)
    {
        if (!HaveReadyPlayers()) { ReadySince = -1.f; return; }
        if (ReadySince < 0.f) ReadySince = Now;
        if (Now - ReadySince >= 3.f) StartRound();
        return;
    }
    if (!IsRoundPlaying()) return;
    if (GS->GetRemainingSeconds() <= 0.f)
    {
        AWRPlayerState* P1 = GS->GetPlayerForTeam(0);
        AWRPlayerState* P2 = GS->GetPlayerForTeam(1);
        const int32 S1 = P1 ? P1->Captures : 0;
        const int32 S2 = P2 ? P2->Captures : 0;
        FinishRound(WRRules::WinnerAtTimeout(S1, S2), TEXT("Time limit"));
        return;
    }
    if (GS->Round.Wave < FMath::Max(1, TotalWaves) && Now - StartedAt >= GS->Round.Wave * FMath::Max(1.f, WaveInterval)) BeginNextWave();
    SpawnPending();
}
void AWRGameMode::BeginNextWave()
{
    if (!IsRoundPlaying()) return;
    AWRGameState* GS = GetGameState<AWRGameState>();
    ++GS->Round.Wave;
    PendingShy += FMath::Max(0, ShyPerWave);
    // This is a per-wave allocation, NOT constant automatic refilling after taming.
    PendingAggressive = WRRules::WildSpawnBudget(AggressivePerWave, CountCreatures(false), MaxWildAggressive);
    GS->OnRep_Round();
    GS->ForceNetUpdate();
    UE_LOG(LogTemp, Log, TEXT("WR Wave %d: pending shy=%d, wild aggressive=%d"), GS->Round.Wave, PendingShy, PendingAggressive);
    SpawnPending();
}
int32 AWRGameMode::CountCreatures(bool bShy) const
{
    int32 Count = 0;
    for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
    {
        if (!It->IsAlive() || It->CreatureState != EWRCreatureState::Wild) continue;
        if ((It->Kind == EWRCreatureKind::Shy) == bShy) ++Count;
    }
    return Count;
}
void AWRGameMode::SpawnPending()
{
    // A collision/nav failure keeps its quota; retry next 0.5 second tick.
    for (int32 Attempt = 0; Attempt < 4 && PendingShy > 0 && CountCreatures(true) < MaxActiveShy; ++Attempt)
    { if (SpawnOne(true)) --PendingShy; }
    for (int32 Attempt = 0; Attempt < 2 && PendingAggressive > 0 && CountCreatures(false) < MaxWildAggressive; ++Attempt)
    { if (SpawnOne(false)) --PendingAggressive; }
}
bool AWRGameMode::SpawnOne(bool bShy)
{
    const TSubclassOf<AWRCreature> Class = bShy ? ShyCreatureClass : AggressiveCreatureClass;
    if (!Class) return false;
    TArray<AActor*> Points;
    UGameplayStatics::GetAllActorsWithTag(this, bShy ? TEXT("WR_Shy") : TEXT("WR_Aggressive"), Points);
    if (Points.IsEmpty()) return false;
    Points.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
    int32& Cursor = bShy ? ShyCursor : AggressiveCursor;
    AActor* Point = Points[Cursor++ % Points.Num()];
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Projected, Location;
    if (!Nav || !Nav->ProjectPointToNavigation(Point->GetActorLocation(), Projected, FVector(200.f, 200.f, 500.f))) return false;
    Location = Projected;
    if (SpawnScatterRadius > 0.f && !Nav->GetRandomReachablePointInRadius(Projected.Location, SpawnScatterRadius, Location)) return false;
    const AWRCreature* Defaults = Class.GetDefaultObject();
    const float HalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FTransform Transform(Point->GetActorRotation(), Location.Location + FVector(0.f, 0.f, HalfHeight + 5.f));
    // Deferred spawn ensures Kind is correct before BeginPlay/controller possession.
    AWRCreature* Creature = GetWorld()->SpawnActorDeferred<AWRCreature>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
    if (!Creature) return false;
    Creature->Kind = bShy ? EWRCreatureKind::Shy : EWRCreatureKind::Aggressive;
    Creature->FinishSpawning(Transform);
    return IsValid(Creature);
}
void AWRGameMode::AwardCapture(AWRPlayerState* Scorer)
{
    if (!HasAuthority() || !IsRoundPlaying() || !IsValid(Scorer) || Scorer->TeamId > 1) return;
    ++Scorer->Captures;
    Scorer->OnRep_PublicState();
    Scorer->ForceNetUpdate();
    if (Scorer->Captures >= FMath::Max(1, CaptureGoal)) FinishRound(Scorer->TeamId, TEXT("Capture goal reached"));
}
void AWRGameMode::FinishRound(int32 WinnerTeam, const FString& Reason)
{
    AWRGameState* GS = GetGameState<AWRGameState>();
    if (!GS || GS->Round.Phase != EWRRoundPhase::Playing) return;
    // One struct keeps the result together for RepNotify consumers.
    GS->Round.Phase = EWRRoundPhase::Finished;
    GS->Round.WinnerTeam = WinnerTeam;
    GS->Round.ResultReason = Reason;
    GS->OnRep_Round();
    GS->ForceNetUpdate();
    PendingShy = PendingAggressive = 0;
    for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It) It->FreezeForRoundEnd();
    GetWorldTimerManager().ClearTimer(MatchTimer);
}
void AWRGameMode::HandlePlayerDefeated(AWRCharacter* Victim)
{
    if (!Victim || !IsRoundPlaying()) return;
    APlayerController* PC = Cast<APlayerController>(Victim->GetController());
    if (!PC) return;
    PC->UnPossess();
    Victim->SetLifeSpan(1.f);
    TWeakObjectPtr<APlayerController> WeakPC(PC);
    TWeakObjectPtr<AWRGameMode> WeakThis(this);
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, [WeakPC, WeakThis]()
    {
        if (WeakThis.IsValid() && WeakPC.IsValid() && WeakThis->IsRoundPlaying())
            WeakThis->RestartPlayer(WeakPC.Get());
    }, FMath::Max(1.f, RespawnDelay), false);
}
void AWRGameMode::RestartRound()
{
    const AWRGameState* GS = GetGameState<AWRGameState>();
    if (!HasAuthority() || !GS || GS->Round.Phase != EWRRoundPhase::Finished) return;
    const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
    GetWorld()->ServerTravel(Map + TEXT("?listen"));
}
