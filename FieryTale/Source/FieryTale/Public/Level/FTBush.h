// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FTBush.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;

UCLASS()
class FIERYTALE_API AFTBush : public AActor
{
    GENERATED_BODY()

public:
    AFTBush();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void RevealOccupant(APawn* Occupant);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    UFUNCTION()
    void OnRep_Occupants(const TArray<APawn*>& OldOccupants);

    UFUNCTION()
    void OnRep_RevealedOccupants();

private:
    void RestoreInvisibility(APawn* Occupant);
    void SetInvisibilityTag(APawn* Occupant, bool bAddTag);
    void SetPawnVisibilityLocal(APawn* Pawn, bool bVisible);
    void UpdateBushEffects();

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BushMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> OverlapVolume;

    UPROPERTY(ReplicatedUsing = OnRep_Occupants)
    TArray<APawn*> Occupants;

    UPROPERTY(ReplicatedUsing = OnRep_RevealedOccupants)
    TArray<APawn*> RevealedOccupants;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Visual")
    FName OpacityParameterName;

    TMap<APawn*, FTimerHandle> RevealTimers;
};