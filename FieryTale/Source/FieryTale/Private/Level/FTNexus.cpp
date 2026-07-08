// Copyright Epic Games, Inc. All Rights Reserved.
#include "Level/FTNexus.h"
#include "UI/Health/FTHealthWidgetComponent.h"
#include "GameplayTags/FTTags.h"
#include "Core/Game/FTGameMode.h"
#include "Engine/World.h"

AFTNexus::AFTNexus()
{
    NexusTeam = EFTNexusTeam::None; // 진영 상태 초기 비지정 정의
}

void AFTNexus::BeginPlay()
{
    Super::BeginPlay(); // 부모 초기화 루틴 실행

    if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>())
    {
        HealthWidgetComp->UpdateASCBinding(); // UI 화이트아웃 무력화 위젯 동기화 강제 가동
    }
}

FGameplayTag AFTNexus::GetTeamTag() const
{
    return (NexusTeam == EFTNexusTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red; // 팀 식별 태그 선별 반환
}

FGameplayTag AFTNexus::GetStructureTag() const
{
    return FTTags::FTObjectType::Structure_Nexus; // 본진 식별 고유 태그 반환
}

void AFTNexus::NotifyDestroyed()
{
    if (UFTHealthWidgetComponent* HealthWidgetComp = FindComponentByClass<UFTHealthWidgetComponent>())
    {
        HealthWidgetComp->SetVisibility(false); // 게이지 렌더링 즉각 소거
    }

    OnNexusDestroyed(); // 블루프린트 연출 시퀀스 동기화 신호 전달

    if (UWorld* World = GetWorld())
    {
        if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
        {
            GameMode->NexusDestroyed(this); // 게임모드 인스턴스에GameOver 매치 마감 연동 통보
        }
    }
}