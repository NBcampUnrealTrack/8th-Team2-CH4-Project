// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "FT_NormalAttack.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

/**
 * 모든 캐릭터의 기본 공격 어빌리티입니다.
 * 무기 타입에 따라 다른 공격 방식을 사용합니다.
 */
UCLASS()
class FIERYTALE_API UFT_NormalAttack : public UFT_GameplayAbility
{
    GENERATED_BODY()
    
public:
    UFT_NormalAttack();
    
    // 어빌리티를 활성화할 수 있는지 조건을 검사합니다.
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    
    // 어빌리티가 종료될 때 호출됩니다.
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 애니메이션 종료 시 호출되는 콜백입니다. */
    UFUNCTION()
    void OnAttackMontageFinished();
    
    /** 공격 판정을 실행합니다. */
    UFUNCTION(BlueprintCallable, Category = "FieryTale|Combat")
    void ExecuteWeaponHitDetection(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter);

    /** LineTrace 방식 공격 판정 */
    void PerformLineTraceLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward);

    /** 투사체 방식 공격 판정 */
    void SpawnProjectileLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter, const FVector& Start, const FVector& Forward);

    /** 근접 공격 판정 */
    void PerformMeleeLogic(UFT_WeaponData* InWeaponData, AFTPlayerCharacterBase* InCharacter);
    
    // --- 이펙트 클래스 ---
    /** 대미지 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effects")
    TSubclassOf<class UGameplayEffect> BaseDamageEffectClass;

private:
    /** 점사 타이머 핸들 */
    FTimerHandle BurstTimerHandle;

    /** 몽타주 안전 타이머 핸들 */
    FTimerHandle NoMontageSafetyTimerHandle;
};