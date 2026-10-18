#include "WRCharacter.h"
#include "WRRules.h"
#include "GameFramework/DamageType.h"
#include "WRMatch.h"
#include "WRAIController.h"
#include "HealthComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "Net/UnrealNetwork.h"

AWRCharacter::AWRCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    MeleeDamage = 25.f;
    MeleeRange = 200.f;
    MinAttackInterval = 0.8f;
}

void AWRCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        if (UHealthComponent* H = GetWRHealth()) H->Health = FMath::Max(1.f, H->MaxHealth);
        if (!IsA<AWRCreature>()) InvulnerableUntil = GetWorld()->GetTimeSeconds() + RespawnProtectionSeconds;
    }
    OnRep_Team();
}

void AWRCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (HasAuthority() && NewController && NewController->IsPlayerController())
    {
        if (const AWRPlayerState* PS = NewController->GetPlayerState<AWRPlayerState>())
        {
            TeamId = PS->TeamId;
            OnRep_Team();
            ForceNetUpdate();
        }
    }
}

UHealthComponent* AWRCharacter::GetWRHealth() const { return FindComponentByClass<UHealthComponent>(); }
bool AWRCharacter::IsAlive() const { const UHealthComponent* H = GetWRHealth(); return !bDefeated && H && !H->IsDead(); }
FGenericTeamId AWRCharacter::GetGenericTeamId() const { return FGenericTeamId(TeamId); }

bool AWRCharacter::IsEnemy(const AActor* Other) const
{
    const AWRCharacter* Target = Cast<AWRCharacter>(Other);
    if (!IsValid(Target) || Target == this || !Target->IsAlive() || !IsAlive()) return false;
    const AWRCreature* MeCreature = Cast<AWRCreature>(this);
    const AWRCreature* OtherCreature = Cast<AWRCreature>(Target);
    // Shy creatures are capture objectives, never combat targets or attackers.
    if ((MeCreature && MeCreature->Kind == EWRCreatureKind::Shy) ||
        (OtherCreature && OtherCreature->Kind == EWRCreatureKind::Shy)) return false;
    if (TeamId == 255 || Target->TeamId == 255 || TeamId == Target->TeamId) return false;
    if (!MeCreature && !OtherCreature)
    {
        const AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>();
        return GM && GM->bAllowPlayerVsPlayerMelee;
    }
    return true;
}

bool AWRCharacter::HasClearPathTo(const AActor* Other) const
{
    if (!IsValid(Other)) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WRLineOfSight), false, this);
    Params.AddIgnoredActor(Other);
    // Arena walls/fences must block Visibility. Ignore endpoints, not intervening cover.
    return !GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Other->GetActorLocation(), ECC_Visibility, Params);
}

void AWRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
        if (CaptureAction) Input->BindAction(CaptureAction, ETriggerEvent::Started, this, &AWRCharacter::CapturePressed);
}
void AWRCharacter::CapturePressed(const FInputActionValue&) { RequestCapture(); }
void AWRCharacter::RequestCapture() { if (IsLocallyControlled()) ServerTryCapture(); }
void AWRCharacter::RequestAttack() { if (IsLocallyControlled()) ServerMeleeAttack(); }

void AWRCharacter::ServerTryCapture_Implementation()
{
    AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>();
    if (!GM || !GM->IsRoundPlaying() || !IsPlayerControlled() || !IsAlive()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastCaptureTime < CaptureInterval) return;
    LastCaptureTime = Now;
    AWRCreature* Best = nullptr;
    float BestDistance = FMath::Square(CaptureRange);
    for (TActorIterator<AWRCreature> It(GetWorld()); It; ++It)
    {
        if (It->Kind != EWRCreatureKind::Shy || It->CreatureState != EWRCreatureState::Wild || !It->IsAlive()) continue;
        const float Distance = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (Distance <= BestDistance && HasClearPathTo(*It)) { Best = *It; BestDistance = Distance; }
    }
    if (Best) Best->TryCapture(this); // Target is selected on the server, never trusted from client.
}

bool AWRCharacter::ServerMeleeAttack_Validate() { return true; }
void AWRCharacter::ServerHeal_Implementation() { /* Lab debug healing deliberately disabled in WR matches. */ }

void AWRCharacter::ServerMeleeAttack_Implementation()
{
    const AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>();
    if (!HasAuthority() || !GM || !GM->IsRoundPlaying() || !IsAlive()) return;
    const AWRCreature* Creature = Cast<AWRCreature>(this);
    if (Creature && Creature->Kind == EWRCreatureKind::Shy) return;
    const float Now = GetWorld()->GetTimeSeconds();
    // Ordinary repeated input is ignored, not treated as a validation failure/disconnect.
    if (Now - LastAttackTime < MinAttackInterval) return;
    LastAttackTime = Now;
    AWRCharacter* Best = nullptr;
    float BestDistance = FMath::Square(MeleeRange);
    const float CosAngle = FMath::Cos(FMath::DegreesToRadians(MeleeHalfAngle));
    for (TActorIterator<AWRCharacter> It(GetWorld()); It; ++It)
    {
        if (!IsEnemy(*It)) continue;
        const FVector To = It->GetActorLocation() - GetActorLocation();
        if (To.SizeSquared() > BestDistance || FVector::DotProduct(GetActorForwardVector(), To.GetSafeNormal()) < CosAngle) continue;
        if (!HasClearPathTo(*It)) continue;
        Best = *It;
        BestDistance = To.SizeSquared();
    }
    FVector Impact = GetActorLocation() + GetActorForwardVector() * MeleeRange;
    if (Best)
    {
        Impact = Best->GetActorLocation();
        UGameplayStatics::ApplyDamage(Best, MeleeDamage, GetController(), this, UDamageType::StaticClass());
    }
    // Do not let AI investigate its own footsteps/swings.
    if (IsPlayerControlled()) UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.f, this, SwingNoiseRange, TEXT("Swing"));
    MulticastWRSwing(Impact);
}

float AWRCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    const AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>();
    if (!HasAuthority() || !GM || !GM->IsRoundPlaying() || !IsAlive() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f) return 0.f;
    if (GetWorld()->GetTimeSeconds() < InvulnerableUntil) return 0.f;
    if (const AWRCreature* C = Cast<AWRCreature>(this)) if (C->Kind == EWRCreatureKind::Shy) return 0.f;
    AWRCharacter* Attacker = Cast<AWRCharacter>(EventInstigator ? EventInstigator->GetPawn() : DamageCauser);
    if (Attacker && !Attacker->IsEnemy(this)) return 0.f;
    // Bypass ArenaGameCharacter's kill-score/death pipeline; keep the engine damage event.
    const float Applied = ACharacter::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (Applied <= 0.f) return 0.f;
    UHealthComponent* H = GetWRHealth();
    H->ApplyDamage(Applied);
    if (H->IsDead()) { HandleDefeated(EventInstigator); }
    else if (Attacker)
    {
        UAISense_Damage::ReportDamageEvent(GetWorld(), this, Attacker, Applied, Attacker->GetActorLocation(), GetActorLocation());
    }
    ForceNetUpdate();
    return Applied;
}
void AWRCharacter::MarkDefeated()
{
    bDefeated = true;
    SetCanBeDamaged(false);
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OnRep_Defeated();
    ForceNetUpdate();
}
void AWRCharacter::HandleDefeated(AController*)
{
    MarkDefeated();
    if (AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>()) GM->HandlePlayerDefeated(this);
}
void AWRCharacter::OnRep_Team() { OnTeamVisualChanged(); }
void AWRCharacter::OnRep_Defeated()
{
    if (bDefeated)
    {
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        OnDefeatedVisual();
    }
}
void AWRCharacter::MulticastWRSwing_Implementation(FVector ImpactLocation) { OnAttackVisual(ImpactLocation); }
void AWRCharacter::FreezeForRoundEnd()
{
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
        if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Wild Rivals round finished"));
    }
}
void AWRCharacter::DoMove(float Right, float Forward)
{
    const AWRGameState* GS = GetWorld()->GetGameState<AWRGameState>();
    if (!bDefeated && (!GS || GS->Round.Phase != EWRRoundPhase::Finished)) Super::DoMove(Right, Forward);
}
void AWRCharacter::DoJumpStart() { if (!bDefeated) Super::DoJumpStart(); }

void AWRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWRCharacter, TeamId);
    DOREPLIFETIME(AWRCharacter, bDefeated);
}

AWRCreature::AWRCreature()
{
    TeamId = 2;
    AIControllerClass = AWRAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    RespawnProtectionSeconds = 0.f;
    MeleeDamage = 15.f;
    MinAttackInterval = 1.2f;
}
void AWRCreature::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->MaxWalkSpeed = Kind == EWRCreatureKind::Shy ? ShySpeed : AggressiveSpeed;
    OnRep_CreatureState();
}
AWRCharacter* AWRCreature::GetOwnerCharacter() const
{
    return IsValid(OwnerPlayer) ? Cast<AWRCharacter>(OwnerPlayer->GetPawn()) : nullptr;
}
bool AWRCreature::TryCapture(AWRCharacter* Player)
{
    AWRGameMode* GM = GetWorld()->GetAuthGameMode<AWRGameMode>();
    AWRPlayerState* PS = Player ? Player->GetPlayerState<AWRPlayerState>() : nullptr;
    if (!HasAuthority() || !GM || !GM->IsRoundPlaying() || !PS || !Player->IsPlayerControlled() || !Player->IsAlive() ||
        Kind != EWRCreatureKind::Shy || CreatureState != EWRCreatureState::Wild || !IsAlive()) return false;
    if (FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Player->CaptureRange) || !Player->HasClearPathTo(this)) return false;
    // Commit BEFORE awarding: a second request cannot award the same creature.
    CreatureState = EWRCreatureState::Resolved;
    GM->AwardCapture(PS);
    DestroyBodyAndController();
    return true;
}
void AWRCreature::HandleDefeated(AController* InstigatorController)
{
    if (CreatureState == EWRCreatureState::Resolved) return;
    AWRPlayerState* Winner = nullptr;
    if (InstigatorController)
    {
        Winner = InstigatorController->GetPlayerState<AWRPlayerState>();
        // Companion landing the last hit credits its owner, but the one-companion cap still applies.
        if (!Winner)
            if (const AWRCreature* Attacker = Cast<AWRCreature>(InstigatorController->GetPawn())) Winner = Attacker->OwnerPlayer;
    }
    if (Winner && WRRules::CanRecruitCompanion(
        CreatureState == EWRCreatureState::Wild && Kind == EWRCreatureKind::Aggressive,
        Winner->TeamId, IsValid(Winner->Companion)))
    {
        BecomeCompanion(Winner);
        return;
    }
    RemoveFromMatch(); // No score for aggressive creatures; tamed creatures never swap teams again.
}
void AWRCreature::BecomeCompanion(AWRPlayerState* NewOwner)
{
    OwnerPlayer = NewOwner;
    TeamId = NewOwner->TeamId;
    CreatureState = EWRCreatureState::Tamed;
    GetWRHealth()->Health = FMath::Max(1.f, GetWRHealth()->MaxHealth);
    InvulnerableUntil = GetWorld()->GetTimeSeconds() + 0.5f;
    NewOwner->Companion = this;
    NewOwner->OnRep_PublicState();
    NewOwner->ForceNetUpdate();
    GetCharacterMovement()->MaxWalkSpeed = CompanionSpeed;
    if (AWRAIController* AI = Cast<AWRAIController>(GetController())) AI->ResetAfterTeamChange();
    OnRep_Team();
    OnRep_CreatureState();
    ForceNetUpdate();
}
void AWRCreature::OnRep_CreatureState() { OnCreatureVisualChanged(); }
void AWRCreature::RemoveFromMatch()
{
    if (!HasAuthority()) return;
    CreatureState = EWRCreatureState::Resolved;
    if (IsValid(OwnerPlayer) && OwnerPlayer->Companion == this)
    {
        OwnerPlayer->Companion = nullptr;
        OwnerPlayer->OnRep_PublicState();
        OwnerPlayer->ForceNetUpdate();
    }
    MarkDefeated();
    DestroyBodyAndController();
}
void AWRCreature::DestroyBodyAndController()
{
    AController* OldController = GetController();
    if (AAIController* AI = Cast<AAIController>(OldController))
    {
        AI->StopMovement();
        if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Creature removed"));
        AI->UnPossess();
    }
    if (OldController) OldController->Destroy();
    Destroy();
}
void AWRCreature::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority() && IsValid(OwnerPlayer) && OwnerPlayer->Companion == this)
    {
        OwnerPlayer->Companion = nullptr;
        OwnerPlayer->OnRep_PublicState();
        OwnerPlayer->ForceNetUpdate();
    }
    Super::EndPlay(Reason);
}

void AWRCreature::ChangeToFleeSpeed()
{
    GetCharacterMovement()->MaxWalkSpeed = ShyFleeSpeed;
    
    UE_LOG(LogTemp, Warning, TEXT("%s | Flee speed: %.1f | Actual MaxWalkSpeed: %.1f"),
    *GetName(),
    ShyFleeSpeed,
    GetCharacterMovement()->MaxWalkSpeed);
}

void AWRCreature::ChangeToShySpeed()
{
    GetCharacterMovement()->MaxWalkSpeed = ShySpeed;
}

void AWRCreature::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWRCreature, Kind);
    DOREPLIFETIME(AWRCreature, CreatureState);
    DOREPLIFETIME(AWRCreature, OwnerPlayer);
}
