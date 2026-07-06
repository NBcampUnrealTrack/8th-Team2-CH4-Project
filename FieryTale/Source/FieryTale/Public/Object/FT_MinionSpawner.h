// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "FT_MinionSpawner.generated.h"

class AFT_MinionCharacterBase; 

UCLASS()
class FIERYTALE_API AFT_MinionSpawner : public AActor
{
    GENERATED_BODY()
    
public:	
    AFT_MinionSpawner();

protected:
    virtual void BeginPlay() override;

    FTimerHandle WaveTimerHandle;
    FTimerHandle MinionSpawnSubTimerHandle;

    void StartWaveSequence();
    void SpawnSingleMinion();

protected:

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Wave Spec")
    float WaveInterval;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Wave Spec")
    int32 MeleeMinionCountPerWave;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Wave Spec")
    int32 RangeMinionCountPerWave;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Wave Spec")
    float SpawnDelayBetweenMinions;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion Classes")
    TSubclassOf<AFT_MinionCharacterBase> MeleeMinionClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion Classes")
    TSubclassOf<AFT_MinionCharacterBase> RangeMinionClass;
    
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FieryTale | Team GAS")
    FGameplayTag SpawnerTeamTag;

private:
    int32 RemainingMeleeToSpawn;
    int32 RemainingRangeToSpawn;
};