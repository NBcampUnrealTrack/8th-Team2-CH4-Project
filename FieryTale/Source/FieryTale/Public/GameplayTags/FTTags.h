// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace FTTags
{
    namespace FTFaction
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Blue); // 블루 팀
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Red);  // 레드 팀
    }

    namespace FTAbilities
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(NormalAttack);  // 평타
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackSkill);   // 공격 스킬
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(UtillSkill);       // 유틸 스킬
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(UltimateSkill); // 궁극기
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateOnGiven);
    }
    
    namespace FTStates
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invincible);     // 무적
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invisibility);   // 은신
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flying);          // 비행
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);        // 기절
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rooted);         // 속박
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Taunted);      // 도발
       
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Slow);     // 슬로우
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Bleeding); // 출혈 지속 피해
       
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_AttackSkill); 
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_UtilSkill);
       
       // 은탄 스킬(우클릭 보조 공격) 고유 쿨타임 태그
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_RightClick); 
    }
    
    namespace FTCombat
    {
       // 평타 및 스킬 대미지 연산용 핵심 마스터 태그
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage); 

       UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponMode_Aiming);  // 스코프 모드
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Channelling);  // 채널링 상태
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Target_Insignia);    // 시계 토끼 태엽 인장
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Muted);   // 적 포탑 포격 마비 상태
    }

    // ◄◄◄ 가구야 궁극기 에러(LNK2001) 해결을 위한 구조물 식별 네임스페이스 선언
    namespace FTObjectType
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Turret); // 적 포탑 구조물
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Nexus);  // 적 넥서스 구조물
    }
    
    namespace FTAugments
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eligible_Phase1); 
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Eligible_Phase2); 
       
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_RedRidingHood_TopBody); 
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_Aladdin_MercyMirage);   
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_Alice_TimeIsMedicine);  
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Applied_Kaguya_BulwarkHeal);
    }
    
    namespace Events
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(KillScored);
    }
}