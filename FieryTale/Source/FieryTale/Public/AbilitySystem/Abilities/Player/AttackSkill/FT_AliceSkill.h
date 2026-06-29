// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceSkill.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_AliceSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_AliceSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 락온 체크를 위한 타이머 핸들
	FTimerHandle LockOnTimerHandle;

	// 실시간으로 타깃을 스캔하는 가상 함수
	void ScanHackingTargets();

	// 락온된 타깃들을 기억할 배열 (실제 액터 포인터나 FHitResult 관리)
	UPROPERTY()
	TArray<AActor*> TrackedTargets;
};
