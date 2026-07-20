// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "FT_KaguyaBulwarkAbility.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;
class UGameplayEffect;

/**
 * 가구야 공주 RMB 강공격 - 봉래의 탄환 방벽 어빌리티 시스템입니다.
 * 전방 120도 각도의 모든 피해를 소멸시키며 아군 딜러진에게 완벽한 프리딜 구도를 선사하는 핵심 방어 기술입니다.
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaBulwarkAbility : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_KaguyaBulwarkAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 방벽 지속 시간 만료 시 호출되는 콜백입니다. */
    UFUNCTION()
    void ExpireBulwark();

    /** 가드 중단 콜백입니다. */
    UFUNCTION()
    void OnGuardInterrupted();

    /** 다른 스킬이나 평타 사용 시 방벽을 해제하기 위한 콜백입니다. */
    void OnAnyAbilityActivated(UGameplayAbility* ActivatedAbility);

protected:
    /** 방벽 지속 타이머 핸들 */
    FTimerHandle BulwarkDurationTimerHandle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Animation")
    TSoftObjectPtr<class UAnimMontage> SkillMontage;

    // --- 고유 스펙 ---
    /** 최대 지속 시간 (기본값 4.0초) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
    float MaxDuration;


    /** 전방에 생성될 거대 방벽 액터 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
    TSoftClassPtr<AActor> BulwarkShieldClass;

    /** 실제 스폰되어 유지 중인 방벽 액터 포인터 */
    UPROPERTY()
    AActor* SpawnedShieldActor;

    /** 타 스킬 감지용 델리게이트 핸들 보관 */
    FDelegateHandle AbilityActivatedHandle;
};