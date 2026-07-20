// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"

void UFT_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모 클래스의 ActivateAbility를 호출합니다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 시전(Cast) 연출 큐가 등록되어 있다면 시전자 본인의 위치에서 재생합니다.
	if (SkillCastCueTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		FGameplayCueParameters CueParams;
		if (const AActor* Avatar = ActorInfo->AvatarActor.Get())
		{
			CueParams.Location = Avatar->GetActorLocation();
			CueParams.TargetAttachComponent = Avatar->GetRootComponent();
		}
		ActorInfo->AbilitySystemComponent->ExecuteGameplayCue(SkillCastCueTag, CueParams);
	}
}

const FGameplayTagContainer* UFT_GameplayAbility::GetCooldownTags() const
{
	// 인스턴스 고유의 쿨다운 태그 컨테이너를 반환합니다.
	const FGameplayTagContainer* ParentTags = Super::GetCooldownTags();
    
	// 지정된 쿨타임 태그가 유효하지 않으면 부모의 반환값을 사용합니다.
	if (!CooldownTag.IsValid())
	{
		return ParentTags;
	}

	// 인스턴스 고유의 멤버 변수를 수정하기 위해 임시로 상수성을 해제합니다.
	UFT_GameplayAbility* MutableThis = const_cast<UFT_GameplayAbility*>(this);
    
	// 부모 클래스의 공용 쿨다운 태그를 복사합니다.
	FGameplayTagContainer CombinedTags;
	if (ParentTags)
	{
		CombinedTags.AppendTags(*ParentTags);
	}

	// 지정된 쿨타임 태그를 결합합니다.
	CombinedTags.AddTag(CooldownTag);

	// 최종 결합된 컨테이너를 멤버 변수인 RuntimeCooldownTags에 대입하여 반환합니다.
	MutableThis->RuntimeCooldownTags = CombinedTags;
    
	return &RuntimeCooldownTags;
}

void UFT_GameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 런타임에 결합된 쿨다운 태그를 기반으로 쿨타임을 적용합니다.
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}

void UFT_GameplayAbility::ApplyMovementPenalty(float SpeedMultiplier)
{
	if (MovementPenaltyGameplayEffectClass && CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		// 💡 [버그 수정]: 이미 적용 중인 페널티가 있다면 중첩/덮어쓰기로 인한 고장(영구 감속)을 방지하기 위해 먼저 제거합니다.
		if (MovementPenaltyActiveHandle.IsValid())
		{
			RemoveMovementPenalty();
		}

		UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
		FGameplayEffectSpecHandle PenaltySpecHandle = MakeOutgoingGameplayEffectSpec(MovementPenaltyGameplayEffectClass, GetAbilityLevel());
		if (PenaltySpecHandle.IsValid() && PenaltySpecHandle.Data.IsValid())
		{
			PenaltySpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::MovementPenalty, SpeedMultiplier);
			MovementPenaltyActiveHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*PenaltySpecHandle.Data.Get());
		}
	}
}

void UFT_GameplayAbility::RemoveMovementPenalty()
{
	if (MovementPenaltyActiveHandle.IsValid() && CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
		SourceASC->RemoveActiveGameplayEffect(MovementPenaltyActiveHandle);
		MovementPenaltyActiveHandle.Invalidate();
	}
}