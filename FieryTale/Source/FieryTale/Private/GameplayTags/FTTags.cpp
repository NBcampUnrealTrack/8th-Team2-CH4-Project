// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayTags/FTTags.h"

namespace FTTags
{
    namespace FTFaction
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Blue, "FTTags.Team.Blue", "AOS 멀티플레이 블루 팀 진영 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Red, "FTTags.Team.Red", "AOS 멀티플레이 레드 팀 진영 태그");
    }

    namespace FTAbilities
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(NormalAttack, "FTTags.Ability.NormalAttack", "LMB 기본 평타 공격 능력 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackSkill, "FTTags.Ability.AttackSkill", "RMB 보조/강공격 능력 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UtillSkill, "FTTags.Ability.UtillSkill", "Shift 이동/생존 기술 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UltimateSkill, "FTTags.Ability.UltimateSkill", "Q 궁극기 능력 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven, "FTTags.Ability.ActivateOnGiven", "부여 시 즉시 활성화되는 패시브 능력용 태그");
    }
    
    namespace FTStates
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invincible, "FTTags.State.Invincible", "모든 대미지와 CC를 흘리는 무적 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invisibility, "FTTags.State.Invisibility", "적 크로스헤어 반응 및 미니맵 노출을 차단하는 은신 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flying, "FTTags.State.Flying", "알라딘 양탄자 탑승 및 비행 이동 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stunned, "FTTags.State.Stunned", "모든 행동 및 스킬 입력이 잠기는 기절 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rooted, "FTTags.State.Rooted", "조준 사격은 되나 WASD 이동이 차단되는 속박 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taunted, "FTTags.State.Taunted", "가구야 공주에게 도발되어 대상을 강제 조준해야 하는 상태 태그");
       
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Debuff_Slow, "FTTags.State.Debuff.Slow", "이동 속도를 감산시키는 둔화 디버프 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Debuff_Bleeding, "FTTags.State.Debuff.Bleeding", "빨간 망토 늑대 공격에 의한 지속 출혈 피해 태그");
       
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_AttackSkill, "FTTags.State.Cooldown.AttackSkill", "우클릭 스킬에대한 쿨타임 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_UtilSkill, "FTTags.State.Cooldown.UtilSkill", "유틸 스킬에대한 쿨타임 태그");
    }
    
    namespace FTCombat
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponMode_Aiming, "FTTags.Combat.WeaponMode.Aiming", "빨간 망토가 2배율 정조준 사격 중인 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Channelling, "FTTags.Combat.Skill.Channelling", "스킬 선시전/선딜레이 중 고정 상태 (인터럽트 체크용)");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Target_Insignia, "FTTags.Combat.Target.Insignia", "앨리스 시계 토끼에 피격된 대상의 태엽 인장 표식");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Muted, "FTTags.Combat.Structure.Muted", "포탑의 AI 포격 루프를 강제로 중단시키는 마비 태그");
    }
    
    namespace FTAugments
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase1, "FTTags.Augment.Eligible.Phase1", " 1차 증강 선택 자격 획득");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase2, "FTTags.Augment.Eligible.Phase2", " 2차 증강 선택 자격 획득");

       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_RedRidingHood_TopBody, "FTTags.Augment.Applied.Phase1.RedRidingHood.TopBody", "빨간 망토 상체 운동 증강 적용 완료");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Aladdin_MercyMirage, "FTTags.Augment.Applied.Phase1.Aladdin.MercyMirage", "알라딘 자비의 신기루 증강 적용 완료");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Alice_TimeIsMedicine, "FTTags.Augment.Applied.Phase1.Alice.TimeIsMedicine", "앨리스 시간이 약 증강 적용 완료");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Kaguya_BulwarkHeal, "FTTags.Augment.Applied.Phase1.Kaguya.BulwarkHeal", "가구야 대나무 장인의 가업 수호 적용 완료");
     }
    
    namespace Events
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(KillScored, "FTTags.Event.KillScored", "적 처치 성공 시 시스템에 격발되는 이벤트 태그");
    }
}