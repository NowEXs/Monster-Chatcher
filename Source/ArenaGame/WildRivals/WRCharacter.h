#pragma once

#include "CoreMinimal.h"
#include "ArenaGameCharacter.h"
#include "NiagaraSystem.h"
#include "WRCharacter.generated.h"

class UNiagaraSystem;
class AWRPlayerState;
class UInputAction;
class UHealthComponent;

UENUM(BlueprintType)
enum class EWRCreatureKind : uint8 { Shy, Aggressive };
UENUM(BlueprintType)
enum class EWRCreatureState : uint8 { Wild, Tamed, Resolved };

UCLASS()
class ARENAGAME_API AWRCharacter : public AArenaGameCharacter
{
    GENERATED_BODY()
public:
    AWRCharacter();
    UPROPERTY(ReplicatedUsing=OnRep_Team, BlueprintReadOnly, Category="Wild Rivals") uint8 TeamId = 255;
    UPROPERTY(ReplicatedUsing=OnRep_Defeated, BlueprintReadOnly, Category="Wild Rivals") bool bDefeated = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wild Rivals|Capture", meta=(ClampMin="1")) float CaptureRange = 200.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Capture", meta=(ClampMin="0.1")) float CaptureInterval = 0.35f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Input") TObjectPtr<UInputAction> CaptureAction;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Combat", meta=(ClampMin="0")) float RespawnProtectionSeconds = 2.f;
    UFUNCTION(BlueprintCallable, Category="Wild Rivals|Input") void RequestCapture();
    UFUNCTION(BlueprintCallable, Category="Wild Rivals|Input") void RequestAttack();
    UFUNCTION(Server, Reliable) void ServerTryCapture();
    UFUNCTION(BlueprintPure, Category="Wild Rivals") UHealthComponent* GetWRHealth() const;
    UFUNCTION(BlueprintPure, Category="Wild Rivals") bool IsAlive() const;
    UFUNCTION(BlueprintPure, Category="Wild Rivals") bool IsEnemy(const AActor* Other) const;
    UFUNCTION(BlueprintImplementableEvent, Category="Wild Rivals|Visuals") void OnTeamVisualChanged();
    UFUNCTION(BlueprintImplementableEvent, Category="Wild Rivals|Visuals") void OnDefeatedVisual();
    UFUNCTION(BlueprintImplementableEvent, Category="Wild Rivals|Visuals") void OnAttackVisual(FVector ImpactLocation);
    UFUNCTION(NetMulticast, Unreliable) void MulticastWRSwing(FVector ImpactLocation);
    
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Combat")
    TObjectPtr<USoundBase> AttackSound;

    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Combat")
    TObjectPtr<UNiagaraSystem> AttackFX;
    
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Audio")
    TObjectPtr<USoundBase> CaptureSuccessSound;

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastCaptureSuccess(FVector CaptureLocation);
    
    // UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Effects")
    // TObjectPtr<class UNiagaraSystem> MeleeAttackFX;
    
    UFUNCTION() void OnRep_Team();
    UFUNCTION() void OnRep_Defeated();
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void PossessedBy(AController* NewController) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void ServerMeleeAttack_Implementation() override;
    virtual bool ServerMeleeAttack_Validate() override;
    virtual void ServerHeal_Implementation() override;
    virtual void DoMove(float Right, float Forward) override;
    virtual void DoJumpStart() override;
    
    
    bool HasClearPathTo(const AActor* Other) const;
    void FreezeForRoundEnd();
protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void HandleDefeated(AController* InstigatorController);
    void MarkDefeated();
    float InvulnerableUntil = 0.f;
private:
    float LastCaptureTime = -1000.f;
    void CapturePressed(const FInputActionValue& Value);
};

UCLASS()
class ARENAGAME_API AWRCreature : public AWRCharacter
{
    GENERATED_BODY()
public:
    AWRCreature();
    UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category="Wild Rivals|Creature") EWRCreatureKind Kind = EWRCreatureKind::Shy;
    UPROPERTY(ReplicatedUsing=OnRep_CreatureState, BlueprintReadOnly, Category="Wild Rivals|Creature") EWRCreatureState CreatureState = EWRCreatureState::Wild;
    UPROPERTY(ReplicatedUsing=OnRep_CreatureState, BlueprintReadOnly, Category="Wild Rivals|Creature") TObjectPtr<AWRPlayerState> OwnerPlayer;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Movement") float ShySpeed = 340.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Movement") float ShyFleeSpeed = 500.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Movement") float AggressiveSpeed = 410.f;
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Movement") float CompanionSpeed = 540.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wild Rivals|Movement") float FollowDistance = 250.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wild Rivals|Movement") float LeashDistance = 1400.f;
    UFUNCTION(BlueprintPure, Category="Wild Rivals|Creature") AWRCharacter* GetOwnerCharacter() const;
    UFUNCTION(BlueprintImplementableEvent, Category="Wild Rivals|Visuals") void OnCreatureVisualChanged();
    UFUNCTION() void OnRep_CreatureState();
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wild Rivals|Effects")
    TObjectPtr<class UNiagaraComponent> FleeFXComponent;
    
    UPROPERTY(EditDefaultsOnly, Category="Wild Rivals|Audio")
    TObjectPtr<USoundBase> FleeSound;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wild Rivals|Audio")
    TObjectPtr<UAudioComponent> FleeAudioComponent;
    
    
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayFleeSound();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastSetFleeFXActive(bool bActive);
    
    bool TryCapture(AWRCharacter* Player);
    void RemoveFromMatch();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void ChangeToFleeSpeed();
    virtual void ChangeToShySpeed();
    
protected:
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    virtual void HandleDefeated(AController* InstigatorController) override;
private:
    void BecomeCompanion(AWRPlayerState* NewOwner);
    void DestroyBodyAndController();
};
