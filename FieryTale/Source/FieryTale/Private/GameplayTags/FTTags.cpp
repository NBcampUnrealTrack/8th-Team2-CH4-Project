// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayTags/FTTags.h"

namespace FTTags
{
    namespace FTFaction
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Blue, "FTTags.Faction.Team.Blue", "AOS 멀티플레이 블루 팀 진영 피아식별 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Team_Red, "FTTags.Faction.Team.Red", "AOS 멀티플레이 레드 팀 진영 피아식별 태그");
    }

    namespace FTAbilities
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(NormalAttack, "FTTags.Abilities.NormalAttack", "LMB 일반 평타 공격 입력 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackSkill, "FTTags.Abilities.AttackSkill", "RMB 보조 공격 및 강공격 기술 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UtilSkill, "FTTags.Abilities.UtilSkill", "Shift 이동 및 생존 유틸 기술 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UltimateSkill, "FTTags.FTAbilities.UltimateSkill", "Q 궁극기 활성화 입력 매핑 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven, "FTTags.Abilities.ActivateOnGiven", "부여 시 즉시 가동되는 패시브 능력 식별 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Minion_Attack, "FTTags.Abilities.Minion.Attack", "미니언 공격 태그");
    }
    
    namespace FTStates
    {
        namespace Core
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dead, "FTTags.State.Core.Dead", "캐릭터 체력 고갈 사망 상태 레이어 태그 (모든 일반공격 및 스킬 연동 차단)");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Reloading, "FTTags.State.Core.Reloading", "무기 탄환 전량 소모 후 장전 중 상태 레이어 태그 (평타 사격 개입 차단)");
        }

        namespace Debuff
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stunned, "FTTags.State.Debuff.Stunned", "모든 행동 및 스킬 입력이 잠기는 기절 상태이상 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rooted, "FTTags.State.Debuff.Rooted", "조준 and 사격은 가능하나 이동만 차단되는 속박 상태이상 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taunted, "FTTags.State.Debuff.Taunted", "가구야 공주에게 도발되어 대상을 강제 조준 및 추격하는 상태이상 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slow, "FTTags.State.Debuff.Slow", "이동 속도를 대폭 감산시키는 둔화 디버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bleeding, "FTTags.State.Debuff.Bleeding", "늑대 발톱 공격에 노출되어 들어오는 실시간 지속 출혈 피해 태그");
        }

        namespace Buff
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invincible, "FTTags.State.Buff.Invincible", "모든 피해량 and CC 연산을 일체 무력화하는 무적 상태 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invisibility, "FTTags.State.Buff.Invisibility", "적의 조준선 크로스헤어 활성화를 차단하고 미니맵 노출을 지우는 은신 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flying, "FTTags.State.Buff.Flying", "알라딘 마법 양탄자 탑승 및 이동 속도 감산 비행 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(Evading, "FTTags.State.Buff.Evading", "구르기 무적 판정 및 대나무 안개 범위 진입 시 부여되는 회피 기동 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(ShrinkActive, "FTTags.State.Buff.ShrinkActive", "지속 시간 동안 앨리스의 히트박스를 축소하고 이동 속도를 올리는 축소 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(CounterReady, "FTTags.State.Buff.CounterReady", "GEEC 대미지 연산을 즉시 중단시키고 타격을 무효화하는 가구야 반격 가드 태세 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(BambooGroveDeploying, "FTTags.State.Buff.BambooGroveDeploying", "가구야 공주가 대나무 숲 전술 연막을 전개하고 있는 시전 버프 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(HackedMark, "FTTags.State.Buff.HackedMark", "앨리스의 레이더 스캔에 적발되어 해킹 락온 연출이 연동될 타깃 표식 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(AladdinComboActive, "FTTags.State.Buff.AladdinComboActive", "알라딘의 소원 성취 궁극기가 연속 격발 가능한 유효 버프 태그");
        }
       
        namespace Cooldown
        {
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackSkill, "FTTags.State.Cooldown.AttackSkill", "공격 기술 공용 재사용 대기시간 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UtilSkill, "FTTags.State.Cooldown.UtilSkill", "Shift 생존 유틸 기술 공용 재사용 대기시간 제어 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(UltimateSkill, "FTTags.State.Cooldown.UltimateSkill", "Q 궁극기 전용 고유 재사용 대기시간 태그");
            UE_DEFINE_GAMEPLAY_TAG_COMMENT(RightClick, "FTTags.State.Cooldown.RightClick", "우클릭 스킬 전용 고유 재사용 대기시간 태그");
        }
    }
    
    namespace FTCombat
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage, "FTTags.Combat.Damage", "SetByCaller 우체통을 통해 마스터 속성 집합으로 피해량을 실시간 배달하는 메타 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Knockback, "FTTags.Combat.Knockback", "피격 대상에게 물리적 척력 및 밀쳐내기 강도를 전송하는 마스터 넉백 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(UltimateCost, "FTTags.Combat.UltimateCost", "궁극기 시전 시 자원 게이지를 차감 청소 초기화하기 위한 비용 전달용 마스터 태그");

       UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponMode_Aiming, "FTTags.Combat.WeaponMode.Aiming", "빨간 망토가 탄퍼짐 수치를 초기화시키고 정조준 사격 중인 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Channelling, "FTTags.Combat.Skill.Channelling", "스킬의 선딜레이 및 캐스팅 유지 상태를 감시하는 인터럽트용 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Target_Insignia, "FTTags.Combat.Target.Insignia", "앨리스 시계 토끼 공격에 노출된 표적의 시한성 태엽 인장 표식 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Muted, "FTTags.Combat.Structure.Muted", "포탑의 AI 자동 포격 감지 루프를 강제로 정지시키는 마비 제어 태그");
        
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(AnimNotify_Attack, "FTTags.Combat.AnimNotify.Attack", "애니메이션 몽타주 내 노티파이 시점에 사출되어 투사체 및 근접 공격을 격발하는 태그");
    }

    namespace FTObjectType
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Turret, "FTTags.ObjectType.Structure.Turret", "AOS 피아식별 및 군중제어기 필터링을 위한 방어 포탑 구조물 식별 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Nexus, "FTTags.ObjectType.Structure.Nexus", "AOS 승패 판정 및 예외 처리를 위한 본진 넥서스 구조물 식별 태그");
    }
    
    namespace FTMinionRole
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Melee, "FTTags.MinionRole.Melee", "전방 돌격 및 적 영웅 우선 어그로 타겟팅을 가동하는 근접 미니언 역할군 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ranged, "FTTags.MinionRole.Ranged", "후방에서 물리/마법 투사체 장부를 사출하여 지원하는 원거리 미니언 역할군 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Healer, "FTTags.MinionRole.Healer", "동일 진영 내부에서 체력 백분율이 가장 낮은 아군을 우선 치유하는 힐러 미니언 역할군 태그");
    }
    
    namespace FTAugments
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase1, "FTTags.Augment.Eligible.Phase1", "레벨 성장에 따른 카드 증강 선택 자격 획득 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase2, "FTTags.Augment.Eligible.Phase2", "레벨 성장에 따른 카드 증강 선택 자격 획득 태그");

       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_RedRidingHood_TopBody, "FTTags.Augment.Applied.Phase1.RedRidingHood.TopBody", "빨간 망토 전용 상체 운동 선택 증강 적용 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Aladdin_MercyMirage, "FTTags.Augment.Applied.Phase1.Aladdin.MercyMirage", "알라딘 전용 지니 궁극기 피해량 증가 증강 적용 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Alice_TimeIsMedicine, "FTTags.Augment.Applied.Phase1.Alice.TimeIsMedicine", "앨리스 전용 시간이 약 선택 증강 적용 상태 태그");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Kaguya_BulwarkHeal, "FTTags.Augment.Applied.Phase1.Kaguya.BulwarkHeal", "가구야 공주 전용 대나무 장인의 가업 수호 증강 적용 상태 태그");
     }
    
    namespace Events
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(KillScored, "FTTags.Event.KillScored", "적 영웅 혹은 크리처 처치 달성 시 배관망에 사출되는 마스터 이벤트 태그");
    }
    
    namespace FTGameplayCue
    {
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Minion_AttackHit, "GameplayCue.Minion.AttackHit", "미니언 평타 적중 및 투사체 폭발 시 시각/청각 연출 격발용 큐");
       UE_DEFINE_GAMEPLAY_TAG_COMMENT(Debuff_Stunned, "GameplayCue.Debuff.Stunned", "기절 상태 지속 시간 동안 머리 위에 루핑 이펙트를 부착하는 상태 큐");
    }
}