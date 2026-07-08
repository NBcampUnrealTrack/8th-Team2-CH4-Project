#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTNexus.generated.h"

UENUM(BlueprintType)
enum class EFTNexusTeam : uint8
{
	None = 0,
	BlueTeam = 1,
	RedTeam = 2
};

UCLASS()
class FIERYTALE_API AFTNexus : public AFTTowerBase
{
	GENERATED_BODY()

public:
	AFTNexus();

	EFTNexusTeam GetNexusTeam() const { return NexusTeam; }

protected:
	//	베이스가 요구하는 진영/식별/기본체력/파괴통보 제공
	virtual FGameplayTag GetTeamTag() const override;
	virtual FGameplayTag GetStructureTag() const override;
	virtual float GetDefaultMaxHealth() const override { return 5000.f; }
	virtual void NotifyDestroyed() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus Property")
	EFTNexusTeam NexusTeam; // 블루/레드 팀 소속 지정 변수
};
