// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaGameCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Engine/DamageEvents.h"
#include "ArenaGameGameMode.h"
#include "ArenaGame.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Team.h"
#include "ArenaAIController.h"

AArenaGameCharacter::AArenaGameCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	
	// Add Melee;s Sphere Component
	MeleeReach = CreateDefaultSubobject<USphereComponent>(TEXT("MeleeReach"));
	MeleeReach->SetupAttachment(GetMesh());
	MeleeReach->SetSphereRadius(MeleeRange);
	MeleeReach->SetRelativeLocation(FVector(0,0,90));
	MeleeReach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeleeReach->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeleeReach->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	MeleeReach->SetGenerateOverlapEvents(true);
}

FGenericTeamId AArenaGameCharacter::GetGenericTeamId() const
{
	// A body has no side of its own. Ask whoever is driving it.
	if (const IGenericTeamAgentInterface* Brain =
			Cast<IGenericTeamAgentInterface>(GetController()))
	{
		return Brain->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

void AArenaGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AArenaGameCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AArenaGameCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AArenaGameCharacter::Look);
		
		// BLANK: bind IA_Attack to OnAttack, firing once on press.
		// hint: EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AArenaGameCharacter::OnAttack);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AArenaGameCharacter::OnAttack);
		
		EnhancedInputComponent->BindAction(HealAction, ETriggerEvent::Started, this, &AArenaGameCharacter::OnHeal);
	}
	else
	{
		UE_LOG(LogArenaGame, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AArenaGameCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AArenaGameCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AArenaGameCharacter::OnAttack(const FInputActionValue& Value)
{
	// Always call the Server RPC. If we ARE the server (host),
	// it just runs locally; if we are a client, it routes up.
	ServerMeleeAttack();
}

void AArenaGameCharacter::OnHeal(const FInputActionValue& Value)
{
	ServerHeal();          // client asks; runs locally if we are the host
}

void AArenaGameCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AArenaGameCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AArenaGameCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AArenaGameCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AArenaGameCharacter::ServerHeal_Implementation()
{
	// Runs on the SERVER. Authority lives here, so the heal holds.
	if (HealthComponent)
	{
		HealthComponent->Heal(5.f);  // amount is the instructor's choice
	}
}

bool AArenaGameCharacter::ServerHeal_Validate()
{
	return true;
}

bool AArenaGameCharacter::ServerMeleeAttack_Validate()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < MinAttackInterval * 0.9f)
	{
		UE_LOG(LogTemp, Warning, TEXT("REFUSED: attack arrived too soon from %s"), *GetName());
		return false;
	}
	
	return true; // no parameters to bounds-check this week
}

void AArenaGameCharacter::ServerMeleeAttack_Implementation()
{
	// WK 11
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < MinAttackInterval) { return; }
	LastAttackTime = Now;
	
	// We are on the SERVER here. Gather candidates in reach.
	TArray<AActor*> Overlapping;
	MeleeReach->GetOverlappingActors(Overlapping,
	AArenaGameCharacter::StaticClass());
		
	TSet<AActor*> AlreadyHit;          // per-swing double-hit guard
	const FVector Origin  = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const float CosThresh = FMath::Cos(
	FMath::DegreesToRadians(MeleeHalfAngle));
		
	bool bAnyHit = false;
	FVector HitLocation = Origin + Forward * MeleeRange; // whiff fallback
		
	for (AActor* T : Overlapping)
	{
		if (!T || T == this) continue;
			
		// BLANK 1: skip a target already hit this swing.
		// hint: AlreadyHit.Contains(T)
		if (AlreadyHit.Contains(T)) continue;
			
		const FVector To = T->GetActorLocation() - Origin;
		const float Dist = To.Size();
			
		// BLANK 2: range gate -- skip anything out of reach.
		// hint: Dist > MeleeRange
		if (Dist > MeleeRange) continue;
			
		const FVector Dir = To.GetSafeNormal();
			
		// BLANK 3: cone gate -- skip anything outside the swing arc.
		// hint: FVector::DotProduct(Forward, Dir) < CosThresh
		if (FVector::DotProduct(Forward, Dir) < CosThresh) continue;
			
		// BLANK 4: remember this target so this swing can't hit it again.
		// hint: AlreadyHit.Add(T);
		/* TODO: copy the hint */
		AlreadyHit.Add(T);
			
		// // WEEK 6 -- remove and replace with WEEK 7
		// // Server calls your Week 3 function on the target's HealthComponent.
		// if (UHealthComponent* HC = T->FindComponentByClass<UHealthComponent>())
		// {
		// 	HC->ApplyDamage(MeleeDamage);
		// 	UE_LOG(LogTemp, Warning, TEXT("DAMAGED %s -> %f"), *T->GetName(), HC->GetHealthPercent());   // temporary
		// }
		
		// WK 11, stop hitting the own's team
		if (const IGenericTeamAgentInterface* Me = Cast<IGenericTeamAgentInterface>(GetController()))
		{
			if (Me->GetTeamAttitudeTowards(*T) != ETeamAttitude::Hostile)
			{
				continue;   // not an enemy -- do not hit them
			}
		}
		
		// WEEK 7 -- the one damage line (UE pipeline)
		UGameplayStatics::ApplyDamage(T, MeleeDamage, 
			GetController(),				// WHO: the instigator
			this,							// WHAT: the causer
			UDamageType::StaticClass());	// KIND: default damage type
			
		if (!bAnyHit) // remember the first confirmed hit for the FX
		{
			bAnyHit = true;
			// BLANK 5: record where the swing connected.
			// hint: HitLocation = T->GetActorLocation();
			/* TODO: copy the hint */
			HitLocation = T->GetActorLocation();
		}
	}
		
	//WK10:
	UAISense_Hearing::ReportNoiseEvent( GetWorld(),
										GetActorLocation(), 
										1.f, 
										this, 
										SwingNoiseRange,
										TEXT("Swing"));
	
	// ONE multicast per swing (see the throttle note below).
	MulticastPlayHitFX(HitLocation);
}

float AArenaGameCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	
	// 1) Engine bookkeeping FIRST -- never skip Super.
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	
	// WK 10
	UAISense_Damage::ReportDamageEvent(GetWorld(), this,
		EventInstigator ? EventInstigator->GetPawn() : nullptr,
		DamageAmount, GetActorLocation(),
		GetActorLocation());

	// WK 11
	if (AArenaAIController* AICon = Cast<AArenaAIController>(GetController()))
	{
		AICon->ReportTrouble(EventInstigator ? EventInstigator->GetPawn() : nullptr);
	}
	 
	// 2) A body cannot be killed twice.
	// BLANK 3: stop here if there is no component, or it is already dead.
	// hint: if (!HealthComponent || HealthComponent->IsDead())
	if (!HealthComponent || HealthComponent->IsDead()) { return Applied; }

	// 3) Route into YOUR Week 3 component. Its HasAuthority guard
	//    stays -- the pipeline adds no networking of its own.
	// BLANK 4: apply the damage through the component you already own.
	// hint: HealthComponent->ApplyDamage(DamageAmount);
	HealthComponent->ApplyDamage(DamageAmount);

	// 4) Did this damage kill? Tell the rules authority WHO did it.
	if (HealthComponent->IsDead())
	{
		if (AArenaGameGameMode* GM =
				GetWorld()->GetAuthGameMode<AArenaGameGameMode>())
		{
			GM->OnCharacterKilled(EventInstigator, GetController());
		}
	}

	return Applied;
}

void AArenaGameCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (!HasAuthority()) { return; }
	if (!IsPlayerControlled()) { return; }
	if (GetVelocity().SizeSquared() < 1.0f) { return;}
	
	FootstepTimer += DeltaSeconds;
	if (FootstepTimer < FootstepInterval) {return;}
	FootstepTimer = 0.0f;
	
	UAISense_Hearing::ReportNoiseEvent( GetWorld(), GetActorLocation(), 1.0f, this,FootstepRange, TEXT("Footstep"));
	
}

void AArenaGameCharacter::MulticastPlayHitFX_Implementation(
FVector HitLocation)
{
	// Runs on server + all clients. COSMETIC ONLY -- no damage here.
	UE_LOG(LogTemp, Log, TEXT("Hit FX on %s at %s"), *GetName(), *HitLocation.ToString());
}
