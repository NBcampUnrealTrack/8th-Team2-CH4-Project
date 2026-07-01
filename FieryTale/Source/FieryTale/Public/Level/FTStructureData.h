#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FTStructureData.generated.h"

// 포탑 및 넥서스의 초기 스탯 데이터를 관리하는 테이블 구조체
USTRUCT(BlueprintType)
struct FFTStructureData : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxHealth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float AttackDamage = 0.0f;
};