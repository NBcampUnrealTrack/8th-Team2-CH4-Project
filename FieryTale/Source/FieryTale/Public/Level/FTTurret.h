#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTTurret.generated.h"

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8
{
	None = 0, // 소속 없음
	BlueTeam = 1, // 블루 팀
	RedTeam = 2 // 레드 팀
};

UENUM(BlueprintType)
enum class EFTTurretPosition : uint8
{
	None = 0, // 위치 없음
	Left = 1, // 좌측
	Right = 2 // 우측
};

UCLASS()
class FIERYTALE_API AFTTurret : public AFTTowerBase
{
	GENERATED_BODY()

public:
	AFTTurret();

	EFTTurretTeam GetTurretTeam() const { return TurretTeam; } // 포탑 팀 반환
	EFTTurretPosition GetTurretPosition() const { return TurretPosition; } // 포탑 위치 반환

protected:
	//	베이스가 요구하는 진영/식별/기본체력/파괴통보 제공
	virtual FGameplayTag GetTeamTag() const override;
	virtual FGameplayTag GetStructureTag() const override;
	virtual float GetDefaultMaxHealth() const override { return 1000.f; }
	virtual void NotifyDestroyed() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretTeam TurretTeam; // 포탑 소속 팀

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretPosition TurretPosition; // 포탑 스폰 위치
};
