// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BenchmarkActorBase.h"
#include "BenchmarkActor_Cpp.generated.h"

UCLASS()
class ARENAGAME_API ABenchmarkActor_Cpp : public ABenchmarkActorBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABenchmarkActor_Cpp();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
