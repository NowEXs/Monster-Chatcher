// Fill out your copyright notice in the Description page of Project Settings.


#include "BenchmarkActor_Cpp.h"


// Sets default values
ABenchmarkActor_Cpp::ABenchmarkActor_Cpp()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABenchmarkActor_Cpp::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABenchmarkActor_Cpp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// BLANK: run the shared loop once per frame, in C++.
	// hint: RunWork();
	/* TODO 2: one line -- copy the hint */
	RunWork();
}

