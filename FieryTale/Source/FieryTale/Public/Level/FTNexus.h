// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTNexus.generated.h"

UENUM(BlueprintType)
enum class EFTNexusTeam : uint8
{
    None = 0, // 진영 없음
    BlueTeam = 1, // 블루팀 진영
    RedTeam = 2 // 레드팀 진영
};

UCLASS()
class FIERYTALE_API AFTNexus : public AFTTowerBase
{
    GENERATED_BODY()

public:
    AFTNexus(); // 생성자

    EFTNexusTeam GetNexusTeam() const { return NexusTeam; } // 넥서스 소속 팀 속성 데이터 반환

protected:
    virtual void BeginPlay() override; // 초기화 루틴

    virtual FGameplayTag GetTeamTag() const override; // 팀 식별 데이터 반환 오버라이드
    virtual FGameplayTag GetStructureTag() const override; // 구조물 종류 데이터 반환 오버라이드
    virtual float GetDefaultMaxHealth() const override { return 5000.f; } // 넥서스 기본 최대 내구도 오버라이드
    virtual void NotifyDestroyed() override; // 파괴 시점 특화 규칙 통보 오버라이드

    UFUNCTION(BlueprintImplementableEvent, Category = "Chaos")
    void OnNexusDestroyed(); // 블루프린트 그래픽 연출 트리거 가상 이벤트

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus Property")
    EFTNexusTeam NexusTeam; // 편집 가능한 진영 구분 데이터 변수
};