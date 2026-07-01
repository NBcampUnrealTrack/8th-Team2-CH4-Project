// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayTags/FTTags.h"

namespace FTTags
{
    namespace FTFaction
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Blue, "Faction.Team.Blue", "AOS 멀티플레이 블루 팀 진영 피아식별 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Red, "Faction.Team.Red", "AOS 멀티플레이 레드 팀 진영 피아식별 태그");
    }

    namespace FTAbilities
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(NormalAttack, "Abilities.NormalAttack", "LMB 일반 평타 공격 입력 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackSkill, "Abilities.AttackSkill", "RMB 보조 공격 및 강공격 기술 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UtillSkill, "Abilities.UtillSkill", "Shift 이동 및 생존 유틸 기술 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UltimateSkill, "Abilities.UltimateSkill", "Q 궁극기 활성화 입력 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven, "Abilities.ActivateOnGiven", "부여 시 즉시 가동되는 패시브 능력 식별 태그");
    }
    
    namespace FTStates
    {
        // [해로운 제어 상태이상 / 하드 CC 디버프 계층]
        namespace Debuff
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stunned, "State.Debuff.Stunned", "모든 행동 및 스킬 입력이 잠기는 기절 상태이상 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rooted, "State.Debuff.Rooted", "조준과 사격은 가능하나 이동만 차단되는 속박 상태이상 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taunted, "State.Debuff.Taunted", "가구야 공주에게 도발되어 대상을 강제 조준 및 추격하는 상태이상 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slow, "State.Debuff.Slow", "이동 속도를 대폭 감산시키는 둔화 디버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bleeding, "State.Debuff.Bleeding", "늑대 발톱 공격에 노출되어 들어오는 실시간 지속 출혈 피해 태그");
        }

        // [이로운 버프 / 능력치 제어 계층]
        namespace Buff
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invincible, "State.Buff.Invincible", "모든 피해량과 CC 연산을 일체 무력화하는 무적 상태 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invisibility, "State.Buff.Invisibility", "적의 조준선 크로스헤어 활성화를 차단하고 미니맵 노출을 지우는 은신 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flying, "State.Buff.Flying", "알라딘 마법 양탄자 탑승 및 이동 속도 30퍼센트 감산 비행 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Evading, "State.Buff.Evading", "구르기 무적 판정 및 대나무 안개 범위 진입 시 부여되는 회피 기동 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(ShrinkActive, "State.Buff.ShrinkActive", "2초 동안 앨리스의 히트박스를 축소하고 이동 속도를 50퍼센트 올리는 축소 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(CounterReady, "State.Buff.CounterReady", "GEEC 대미지 연산을 즉시 중단시키고 타격을 무효화하는 가구야 반격 가드 태세 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(BambooGroveDeploying, "State.Buff.BambooGroveDeploying", "가구야 공주가 대나무 숲 전술 연막을 전개하고 있는 시전 버프 태그");
        }
       
        // [재사용 대기시간 자원 관리 계층]
        namespace Cooldown
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackSkill, "State.Cooldown.AttackSkill", "보조 공격 기술 공용 재사용 대기시간 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UtilSkill, "State.Cooldown.UtilSkill", "Shift 생존 유틸 기술 공용 재사용 대기시간 15초 제어 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(RightClick, "State.Cooldown.RightClick", " 우클릭 스킬 전용 9초 고유 재사용 대기시간 태그");
        }
    }
    
    namespace FTCombat
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage, "Combat.Damage", "SetByCaller 우체통을 통해 마스터 속성 집합으로 피해량을 실시간 배달하는 메타 태그");

       UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponMode_Aiming, "Combat.WeaponMode.Aiming", "빨간 망토가 탄퍼짐 수치를 초기화시키고 2배율 정조준 사격 중인 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Channelling, "Combat.Skill.Channelling", "스킬의 선딜레이 및 캐스팅 유지 상태를 감시하는 인터럽트용 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Target_Insignia, "Combat.Target.Insignia", "앨리스 시계 토끼 공격에 노출된 표적의 시한성 태엽 인장 표식 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Muted, "Combat.Structure.Muted", "포탑의 AI 자동 포격 감지 루프를 강제로 정지시키는 마비 제어 태그");
    }

    namespace FTObjectType
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Turret, "ObjectType.Structure.Turret", "AOS 피아식별 및 군중제어기 필터링을 위한 방어 포탑 구조물 식별 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Nexus, "ObjectType.Structure.Nexus", "AOS 승패 판정 및 예외 처리를 위한 본진 넥서스 구조물 식별 태그");
    }
    
    namespace FTAugments
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase1, "Augment.Eligible.Phase1", "레벨 성장에 따른 1차 카드 증강 선택 자격 획득 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase2, "Augment.Eligible.Phase2", "레벨 성장에 따른 2차 카드 증강 선택 자격 획득 태그");

       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_RedRidingHood_TopBody, "Augment.Applied.Phase1.RedRidingHood.TopBody", "빨간 망토 전용 상체 운동 선택 증강 적용 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Aladdin_MercyMirage, "Augment.Applied.Phase1.Aladdin.MercyMirage", "알라딘 전용 지니 궁극기 피해량 2배 증가 자비의 신기루 증강 적용 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Alice_TimeIsMedicine, "Augment.Applied.Phase1.Alice.TimeIsMedicine", "앨리스 전용 시간이 약 선택 증강 적용 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Kaguya_BulwarkHeal, "Augment.Applied.Phase1.Kaguya.BulwarkHeal", "가구야 공주 전용 대나무 장인의 가업 수호 증강 적용 상태 태그");
     }
    
    namespace Events
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(KillScored, "Event.KillScored", "적 영웅 혹은 크리처 처치 달성 시 배관망에 사출되는 마스터 이벤트 태그");
    }
}