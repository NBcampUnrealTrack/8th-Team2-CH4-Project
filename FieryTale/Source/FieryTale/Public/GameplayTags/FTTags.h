// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace FTTags
{
    namespace FTFaction
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Blue);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Team_Red);
    }

    namespace FTAbilities
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(NormalAttack);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackSkill);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(UtilSkill);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(UltimateSkill);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateOnGiven);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Minion_Attack);
    }
    
    namespace FTStates
    {
        namespace Core
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reloading);
        }
        
        namespace Debuff
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rooted);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Taunted);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slow);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bleeding);
        }
        
        namespace Buff
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invincible);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invisibility);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flying);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(Evading);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShrinkActive);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(CounterReady);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(BambooGroveDeploying);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(HackedMark);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(AladdinComboActive);
        }
       
        namespace Cooldown
        {
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackSkill);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UtilSkill);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(UltimateSkill);
            UE_DECLARE_GAMEPLAY_TAG_EXTERN(RightClick);
        }
    }
    
    namespace FTCombat
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Knockback);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(UltimateCost);

       UE_DECLARE_GAMEPLAY_TAG_EXTERN(WeaponMode_Aiming);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Channelling);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Target_Insignia);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Muted);
    	
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(AnimNotify_Attack);
    }

    namespace FTObjectType
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Turret);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Structure_Nexus);
    }
    
    namespace FTMinionRole
    {
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Melee);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ranged);
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(Healer);
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
       UE_DECLARE_GAMEPLAY_TAG_EXTERN(CharacterDeath);
    }
	
}

namespace FTGameplayCue
{
	// 미니언 히트
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Minion_AttackHit);
	// 공용 디버프 
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Rooted);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Taunted);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Slow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Debuff_Knockback);
    	
	//앨리스 
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Alice_AttackHit);
    	
	//카구야
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Kaguya_AttackHit);
    	
	//빨간 망토
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(RedHood_AttackHit);
    	
	//알라딘
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Aladdin_AttackHit);
}