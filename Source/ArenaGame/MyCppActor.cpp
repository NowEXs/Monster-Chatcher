// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCppActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

// Sets default values
AMyCppActor::AMyCppActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	MyRotationSpeed = 45.0f;   // degrees per second
	
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	RootComponent = StaticMesh;

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->SetupAttachment(StaticMesh);
	SphereCollision->SetSphereRadius(120.0f);
}

// Called when the game starts or when spawned
void AMyCppActor::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Warning, TEXT("Hello from MyCppActor (C++)!"));
	
	SetActorLocation(FVector(0.0f, 0.0f, 200.0f));
	StaticMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.2f));
	
	SphereCollision->SetGenerateOverlapEvents(true);
	SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &AMyCppActor::OnOverlapBegin);
}

// Called every frame
void AMyCppActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	FRotator CurrentRotation = GetActorRotation();
	CurrentRotation.Yaw += DeltaTime * MyRotationSpeed;
	SetActorRotation(CurrentRotation);

}

void AMyCppActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && (OtherActor != this))
	{
		 UE_LOG(LogTemp, Warning, TEXT("Overlapped by %s - destroying."),
		 	   *OtherActor->GetName());
		// Destroy();
		ApplyDamage(35.0f);
	}
}

void AMyCppActor::PrintMessageFromCpp(const FString& MessageToPrint)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, MessageToPrint);
	}
}

bool AMyCppActor::IsInAttackRange(FVector TargetLocation, float Range) const
{
	const float DistSq =
		FVector::DistSquared(GetActorLocation(), TargetLocation);
	return DistSq <= Range * Range;
}

bool AMyCppActor::CanSee(FVector TargetLocation) const
{
	const FVector Dir =
		(TargetLocation - GetActorLocation()).GetSafeNormal();

	// total degrees -> cosine of the HALF angle (the * 0.5f is the "half")
	const float CosHalfFOV =
		FMath::Cos(FMath::DegreesToRadians(SightFOVDegrees * 0.5f));

	return FVector::DotProduct(GetActorForwardVector(), Dir) >= CosHalfFOV;
}

void AMyCppActor::ApplyDamage(float Damage) 
{ 
	Health = FMath::Max(0.0f, Health - Damage); // clamp at 0 
	
	UE_LOG(LogTemp, Warning, TEXT("Damage %2.2f"), Health);
	
	FString MessageToPrint = "Damage: Remaining health -> " + FString::Printf(TEXT("%2.2f"), Health);
	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, MessageToPrint);
	
	if (Health <= 0.0f) { Destroy(); }
}