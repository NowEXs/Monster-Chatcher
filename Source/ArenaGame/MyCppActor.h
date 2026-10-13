// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyCppActor.generated.h"

UCLASS()
class ARENAGAME_API AMyCppActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyCppActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyActor")
	float MyRotationSpeed;
	
	UFUNCTION(BlueprintCallable, Category = "MyActor")
	void PrintMessageFromCpp(const FString& MessageToPrint);
	
	UFUNCTION(BlueprintCallable, Category = "MathToolkit")
	bool IsInAttackRange(FVector TargetLocation, float Range) const;
	
	// Total cone width in degrees. 90 = 45 to each side. 360 = sees everything.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MathToolkit",
			  meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float SightFOVDegrees = 90.0f;
	
	UFUNCTION(BlueprintCallable, Category = "MathToolkit")
	bool CanSee(FVector TargetLocation) const;
	
	UFUNCTION(BlueprintCallable, Category = "MathToolkit")
	void ApplyDamage(float Damage);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MathToolkit")
	float Health = 100.0f;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyActor")
	class UStaticMeshComponent* StaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyActor")
	class USphereComponent* SphereCollision;
	
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
						AActor* OtherActor,
						UPrimitiveComponent* OtherComp,
						int32 OtherBodyIndex,
						bool bFromSweep,
						const FHitResult& SweepResult);
};
