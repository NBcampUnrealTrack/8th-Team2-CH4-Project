// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayTags/FTTags.h"

namespace FTTags
{
	namespace FTAbilities
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(NormalAttack, "Ability.NormalAttack", "LMB 기본 평타 공격 능력 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackSkill, "Ability.AttackSkill", "RMB 보조/강공격 능력 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(UtillSkill, "Ability.UtillSkill", "Shift 이동/생존 기술 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(UltimateSkill, "Ability.UltimateSkill", "Q 궁극기 능력 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven, "Ability.ActivateOnGiven", "부여 시 즉시 활성화되는 패시브 능력용 태그");
	}
	
	namespace FTStates
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invincible, "State.Invincible", "모든 대미지와 CC를 흘리는 무적 상태 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invisibility, "State.Invisibility", "적 크로스헤어 반응 및 미니맵 노출을 차단하는 은신 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flying, "State.Flying", "알라딘 양탄자 탑승 및 비행 이동 상태 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stunned, "State.Stunned", "모든 행동 및 스킬 입력이 잠기는 기절 상태 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rooted, "State.Rooted", "조준 사격은 되나 WASD 이동이 차단되는 속박 상태 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taunted, "State.Taunted", "가구야 공주에게 도발되어 대상을 강제 조준해야 하는 상태 태그");
       
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Debuff_Slow, "State.Debuff.Slow", "이동 속도를 감산시키는 둔화 디버프 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Debuff_Bleeding, "State.Debuff.Bleeding", "빨간 망토 늑대 공격에 의한 지속 출혈 피해 태그");
	}
	
	namespace FTCombat
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(WeaponMode_Aiming, "Combat.WeaponMode.Aiming", "빨간 망토가 2배율 정조준 사격 중인 상태 태그");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Channelling, "Combat.Skill.Channelling", "스킬 선시전/선딜레이 중 고정 상태 (인터럽트 체크용)");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Target_Insignia, "Combat.Target.Insignia", "앨리스 시계 토끼에 피격된 대상의 태엽 인장 표식");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Structure_Muted, "Combat.Structure.Muted", "포탑의 AI 포격 루프를 강제로 중단시키는 마비 태그");
	}
	
	namespace FTAugments
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase1, "Augment.Eligible.Phase1", " 1차 증강 선택 자격 획득");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Eligible_Phase2, "Augment.Eligible.Phase2", " 2차 증강 선택 자격 획득");

		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_RedRidingHood_TopBody, "Augment.Applied.Phase1.RedRidingHood.TopBody", "빨간 망토 상체 운동 증강 적용 완료");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Aladdin_MercyMirage, "Augment.Applied.Phase1.Aladdin.MercyMirage", "알라딘 자비의 신기루 증강 적용 완료");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Alice_TimeIsMedicine, "Augment.Applied.Phase1.Alice.TimeIsMedicine", "앨리스 시간이 약 증강 적용 완료");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Applied_Kaguya_BulwarkHeal, "Augment.Applied.Phase1.Kaguya.BulwarkHeal", "가구야 대나무 장인의 가업 수호 적용 완료");
	 }
	
	namespace Events
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(KillScored, "Event.KillScored", "적 처치 성공 시 시스템에 격발되는 이벤트 태그");
	}
}
