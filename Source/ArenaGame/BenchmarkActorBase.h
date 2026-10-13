// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BenchmarkActorBase.generated.h"

UCLASS()
class ARENAGAME_API ABenchmarkActorBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABenchmarkActorBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
	int32 Iterations = 1000;
	
	UFUNCTION(BlueprintCallable, Category = "Benchmark")
	float RunWork();   // the same loop you tested offline
};
