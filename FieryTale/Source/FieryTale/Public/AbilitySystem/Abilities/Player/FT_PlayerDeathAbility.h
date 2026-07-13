// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_PlayerDeathAbility.generated.h"

/**
 * 캐릭터 사망 시 서버가 GameplayEvent(FTTags::Events::CharacterDeath)로 깨우는 마스터 GA 클래스입니다.
 * 아바타(AFTCharacterBase)에 이미 세팅되어 있는 영웅별 DeathMontage를 재생하고,
 * 재생이 끝나면 AFTCharacterBase::FinishDying()을 호출해 후속 정리를 위임합니다.
 */
UCLASS()
class FIERYTALE_API UFT_PlayerDeathAbility : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_PlayerDeathAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 사망 몽타주 종료/중단 시 후속 정리(FinishDying)와 어빌리티 종료를 처리하는 바인딩 함수 */
	UFUNCTION()
	void OnDeathMontageCompletedOrInterrupted();
};
