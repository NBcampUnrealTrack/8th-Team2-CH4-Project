#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTNexus.generated.h"

class UStaticMeshComponent;
class UGeometryCollectionComponent;
class UDataTable;
class USoundBase;
class UNiagaraSystem;
class UCapsuleComponent;

UENUM(BlueprintType)
enum class EFTNexusTeam : uint8
{
    NoneTeam,
    BlueTeam,
    RedTeam
};

UCLASS()
class FIERYTALE_API AFTNexus : public AFTTowerBase
{
    GENERATED_BODY()

public:
    AFTNexus();
    EFTNexusTeam GetNexusTeam() const { return NexusTeam; }
    void SetVulnerable(bool bNewVulnerable);

    UFUNCTION(BlueprintPure, Category = "FieryTale | Structure")
    bool IsVulnerable() const;

protected:
    virtual void BeginPlay() override;
    virtual FGameplayTag GetTeamTag() const override;
    virtual FGameplayTag GetStructureTag() const override;
    virtual float GetDefaultMaxHealth() const override { return 5000.f; }
    virtual void PerformDestructionEffects() override;
    virtual void NotifyGameMode() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Chaos")
    void OnNexusDestroyed();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> NexusMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UGeometryCollectionComponent> DebrisMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
    TObjectPtr<UDataTable> NexusDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
    FName DataRowName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus Property")
    EFTNexusTeam NexusTeam;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<USoundBase> DestructionSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<UNiagaraSystem> DestructionEffect;
};