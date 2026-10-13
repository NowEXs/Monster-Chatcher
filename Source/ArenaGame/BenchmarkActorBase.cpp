// Fill out your copyright notice in the Description page of Project Settings.


#include "BenchmarkActorBase.h"


// Sets default values
ABenchmarkActorBase::ABenchmarkActorBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABenchmarkActorBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABenchmarkActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float ABenchmarkActorBase::RunWork()
{
	FVector Acc = FVector::ZeroVector;            // given
	for (int32 i = 0; i < Iterations; ++i)
	{
		FVector V(i * 1.0f, i * 2.0f, i * 3.0f);  // given
		// BLANK: add this vector's unit direction, scaled by 2, into Acc.
		// hint: Acc = Acc + V.GetSafeNormal() * 2.0f;
		/* TODO 1: one line -- copy the hint */
	}
	return Acc.Size();                            // given
}