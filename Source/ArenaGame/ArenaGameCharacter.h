// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "GenericTeamAgentInterface.h"	// WK11
#include "ArenaGameCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UHealthComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AArenaGameCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	class UHealthComponent* HealthComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;
	
	/** Attack Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AttackAction;
	
	/** Heal Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* HealAction;

public:

	/** Constructor */
	AArenaGameCharacter();	
	virtual FGenericTeamId GetGenericTeamId() const override;   // NEW WK 11

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);
	
	/** Called for attack input */
	void OnAttack(const FInputActionValue& Value);
	
	void OnHeal(const FInputActionValue& Value);
	
	// WK-10
	UPROPERTY(EditDefaultsOnly, Category="AI")
	float FootstepInterval = 0.4f;	// seconds between reports
	
	UPROPERTY(EditDefaultsOnly, Category="AI Perception")
	float FootstepRange = 900.0f;	// how far a step carries
	
	UPROPERTY(EditDefaultsOnly, Category="AI Perception")
	float SwingNoiseRange = 2500.0f;	//how far a swing carries
	
	float FootstepTimer = 0.0f;	// runtime only

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();
	
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	// --- melee tunables (designer-editable) ---
	UPROPERTY(EditAnywhere, Category="Melee") 
	float MeleeRange = 150.f;
	
	UPROPERTY(EditAnywhere, Category="Melee") 
	float MeleeHalfAngle = 60.f;
	
	UPROPERTY(EditAnywhere, Category="Melee") 
	float MeleeDamage = 5.f;
	
	UPROPERTY(VisibleAnywhere, Category="Melee")
	class USphereComponent* MeleeReach;
	
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerMeleeAttack();
	
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerHeal();
	
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitFX(FVector HitLocation);
	
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
	virtual void Tick(float DeltaSeconds) override;
	
protected:
	// WK 11: How long between swing (seconds)
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	float MinAttackInterval = 1.0f;
	
	float LastAttackTime = -1000.0f;		// Not Unreal UPROPERTY
};

