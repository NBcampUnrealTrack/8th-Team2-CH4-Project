// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_HitReactionAbility.generated.h"

class UAnimMontage;

/**
 * 전장의 캐릭터 및 미니언이 피격 이벤트를 수신했을 때
 * 피격 반응 애니메이션 및 제어를 담당하는 어빌리티 클래스입니다.
 */
UCLASS()
class FIERYTALE_API UFT_HitReactionAbility : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_HitReactionAbility();

	/** 어빌리티 활성화 시 호출되는 콜백입니다. */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 피격 몽타주 종료 또는 중단 시 호출되어 어빌리티를 종료합니다. */
	UFUNCTION()
	void OnHitMontageCompletedOrInterrupted();

	/** 피격 시 재생할 애니메이션 몽타주입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
	TObjectPtr<UAnimMontage> HitReactionMontage;
};