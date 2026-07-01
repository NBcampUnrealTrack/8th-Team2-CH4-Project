#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "FTNexus.generated.h"

UENUM(BlueprintType)
enum class EFTNexusTeam : uint8
{
    None = 0,
    BlueTeam = 1,
    RedTeam = 2
};

UCLASS()
class FIERYTALE_API AFTNexus : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
	
public:	
    AFTNexus();

    virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    EFTNexusTeam GetNexusTeam() const { return NexusTeam; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 어트리뷰트셋의 체력이 변경될 때 호출되는 콜백 함수
    void OnHealthChanged(const struct FOnAttributeChangeData& Data);

    // 파괴 연출 상태를 활성화하고 사운드, 이펙트 및 승패 결과를 호출하는 함수
    void DebugDestroyNexus();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> NexusMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AttributeSet> AttributeSet;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
    TObjectPtr<class UDataTable> NexusDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
    FName DataRowName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus Property")
    EFTNexusTeam NexusTeam;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<class USoundBase> DestructionSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
    TObjectPtr<class UParticleSystem> DestructionEffect;

private:
    FTimerHandle AutoDestroyTimerHandle;
    FVector InitialLocation;
    bool bIsDying;
    float DyingTimer;
    float MaxDyingTime;
};