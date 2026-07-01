// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace FTTags
{
    namespace FTFaction
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Blue); // 블루 팀 피아식별
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Red);  // 레드 팀 피아식별
    }

    namespace FTAbilities
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(NormalAttack);  // LMB 일반 평타 공격
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackSkill);   // RMB 보조 공격 기술
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(UtilSkill);    // Shift 이동 및 생존 유틸 기술
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(UltimateSkill); // Q 궁극기 기술
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateOnGiven);
    }
    
    // --- 영웅 및 크리처들의 실시간 상태 제어 마스터 대분류 ---
    namespace FTStates
    {
        // [긴급 보정 완착] .cpp의 문자열 주소지와 자석처럼 바인딩될 마스터 EXTERN 주소선을 개통합니다.
        namespace Core
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);            // 캐릭터 사망 상태 레이어 (평타 및 스킬 연동 차단)
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reloading);       // 무기 장전 중 상태 레이어 (평타 사격 개입 차단)
        }

        // [해로운 제어 상태이상 / 하드 CC 디버프 계층]
        namespace Debuff
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);         // 기절 (조작 불가 / 앨리스 궁극기 연동)
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rooted);          // 속박 (이동 불가 / 늑대 궁극기 연동)
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Taunted);         // 도발
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slow);            // 이동 속도 감소 (가구야 궁극기 80% 슬로우 연동)
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bleeding);        // 출혈 지속 피해 디버프
        }

        // [이로운 버프 / 능력치 제어 계층 - GE 및 스킬 연동 필수 구역]
        namespace Buff
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invincible);      // 완전 무적 상태
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invisibility);    // 순정 은신 상태 (가구야 연막 진입 버프용)
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flying);          // 알라딘: 양탄자 기류 비행 버프 상태
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Evading);         // 빨간망토 구르기: 회피 기동 및 무적 태그 상태
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShrinkActive);    // 앨리스: 토끼의 약 신체 축소 버프 상태
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(CounterReady);    // 가구야: GEEC 데미지를 0으로 분쇄하는 반격 가드 태세
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(BambooGroveDeploying); // 가구야: 대나무 숲 안개 영역 전개 중인 시전 상태
            
            // 알라딘 3단 궁극기 콤보 연사 및 입력 락 차단을 위한 실시간 트래커 태그
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(AladdinComboActive);
        }
       
        // [재사용 대기시간 자원 관리 계층]
        namespace Cooldown
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackSkill);     // 공격 기술 재사용 대기시간
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UtilSkill);       // Shift 유틸 기술 공용 재사용 대기시간 (15초)
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(RightClick);      // RMB 보조 공격 고유 재사용 대기시간 (9초)
        }
    }
    
    namespace FTCombat
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);              // 평타 및 스킬 최종 데미지 연산용 마스터 메타 태그

       UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponMode_Aiming);  // 정조준 스코프 모드 상태 (탄퍼짐 복구 연동)
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Channelling);  // 채널링 시전 유지 상태 (앨리스 궁극기 시전자 락 연동)
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Target_Insignia);    // 시계 토끼 시한성 태엽 인장 표식
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Muted);    // 적 포탑 포격 기능 무력화 상태
    }

    namespace FTObjectType
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Turret);   // 적 포탑 구조물 판정 오브젝트
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Nexus);    // 적 넥서스 본진 구조물 판정 오브젝트
    }
    
    namespace FTAugments
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eligible_Phase1); 
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eligible_Phase2); 
       
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_RedRidingHood_TopBody); 
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_Aladdin_MercyMirage);   // 알라딘 3타 막타 대미지 2배 증강 분기 연동
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_Alice_TimeIsMedicine);  
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_Kaguya_BulwarkHeal);
    }
    
    namespace Events
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(KillScored);
    }
}