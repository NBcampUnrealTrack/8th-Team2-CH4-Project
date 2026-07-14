// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AimingSkill.generated.h"

/**
 * 우클릭 보조/강공격 - 정조준(Aiming) 및 탄퍼짐 압축 스킬
 */
UCLASS()
class FIERYTALE_API UFT_AimingSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_AimingSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 탄퍼짐 복구를 위한 캐릭터 포인터 캐싱 */
	UPROPERTY()
	class AFTPlayerCharacterBase* CachedCharacter;
};