// Fill out your copyright notice in the Description page of Project Settings.


#include "FieryTale/Public/AbilitySystem/Abilities/FT_GameplayAbility.h"

#include "Engine/Engine.h"

void UFT_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                          const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (bDrawDebugs && IsValid(GEngine))
	{
		GEngine->AddOnScreenDebugMessage(-1,3.f,FColor::Cyan,FString::Printf(TEXT("%s Activated"), *GetName()));
	}
}

const FGameplayTagContainer* UFT_GameplayAbility::GetCooldownTags() const
{
	// 지정된 쿨타임 게임플레이 이펙트가 들고 있는 고유 태그 컨테이너를 탐색합니다
	if (CooldownGameplayEffectClass) 
	{
		if (const UGameplayEffect* CooldownGE = CooldownGameplayEffectClass->GetDefaultObject<UGameplayEffect>())
		{
			return &CooldownGE->GetGrantedTags();
		}
	}
	return Super::GetCooldownTags();
}

void UFT_GameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownGameplayEffectClass)
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGameplayEffectClass, GetAbilityLevel());
        
		if (SpecHandle.IsValid())
		{
			// 함수가 리턴하는 ActiveGameplayEffectHandle을 (void)로 캐스팅하여 
			// 컴파일러의 "결과가 사용되지 않았습니다" 경고를 완벽하게 차단합니다.
			(void)ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}
}