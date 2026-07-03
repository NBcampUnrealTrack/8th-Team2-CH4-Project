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

	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void DebugDestroyNexus();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> NexusMesh; // 넥서스 외형 스태틱 메시

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent; // GAS 능력을 처리하는 핵심 컴포넌트

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<class UFT_AttributeSet> AttributeSet; // 체력 및 스탯을 관리하는 어트리뷰트 세트

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<class UDataTable> NexusDataTable; // 스탯 참조용 데이터 테이블 에셋

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName; // 데이터 테이블에서 가져올 행 이름

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus Property")
	EFTNexusTeam NexusTeam; // 블루/레드 팀 소속 지정 변수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<class USoundBase> DestructionSound; // 파괴 시 출력할 사운드 에셋

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<class UParticleSystem> DestructionEffect; // 파괴 시 스폰할 폭발 이펙트 에셋

private:
	FVector InitialLocation;
	bool bIsDying;
	float DyingTimer;
	float MaxDyingTime;
};