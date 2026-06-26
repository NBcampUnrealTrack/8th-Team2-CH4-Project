// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_NormalAttack.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_NormalAttack : public UFT_GameplayAbility
{
	GENERATED_BODY()
	
public:
	UFT_NormalAttack();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	
	UFUNCTION()
	void OnAttackMontageFinished();
	
	/** 실제 격발 타이밍에 호출될 판정 엔진 핵심 함수 */
	void ExecuteWeaponHitDetection(class UFT_WeaponData* WeaponData);

	/** [LineTrace 타입] 즉시 발사 및 헤드샷 연산 */
	void PerformLineTraceLogic(class UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward);

	/** [Projectile 타입] 투사체 사출 (알라딘 3연사 / 앨리스 도탄 대응용 규격) */
	void SpawnProjectileLogic(class UFT_WeaponData* WeaponData, const FVector& Start, const FVector& Forward);

	/** [Melee 타입] 가구야 근접 박스 히트 판정 */
	void PerformMeleeLogic(class UFT_WeaponData* WeaponData);
	
	/** 최종 대미지 수치를 주입하여 타겟의 AttributeSet에 가감할 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fiery|Effect")
	TSubclassOf<UGameplayEffect> BaseDamageEffectClass;
};
