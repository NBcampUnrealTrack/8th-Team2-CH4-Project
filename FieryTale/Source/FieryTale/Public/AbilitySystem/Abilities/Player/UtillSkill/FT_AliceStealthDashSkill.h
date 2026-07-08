// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "GameplayEffectTypes.h" // ◄ FActiveGameplayEffectHandle 참조 배관망을 위해 인클루드 완착
#include "FT_AliceStealthDashSkill.generated.h"

class AFTPlayerCharacterBase;
class UGameplayEffect; // ◄ 전방 선언을 통해 컴파일 속도 최적화

/**
 * 앨리스 Shift 기술 - 토끼의 물약 신체 축소 가속 어빌리티 시스템입니다.
 * 은신 개념은 기획 사양에 따라 전면 제외되었으며, 히트박스 축소와 이동 가속에 집중합니다.
 */
UCLASS()
class FIERYTALE_API UFT_AliceStealthDashSkill : public UFT_GameplayAbility
{
    GENERATED_BODY()

public:
    UFT_AliceStealthDashSkill();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    /** 2초 버프 지속 시간이 끝났을 때 원래 크기와 속도로 되돌릴 안전 수거 콜백 함수 */
    UFUNCTION()
    void OnShrinkBuffFinished();

protected:
    /** 2초 유지를 칼같이 제어하여 런타임 누수를 차단할 버프 타이머 핸들 */
    FTimerHandle ShrinkBuffDurationTimerHandle;

    // --- 앨리스 신체 축소 고유 기획 스펙 데이터 블록 ---
    /** 축소 버프가 정상 유지되는 총 한계 시간 (기본값 2.0초 명세) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float MaxDuration;

    /** 캐릭터 비주얼 메쉬 스케일을 줄이기 위한 목표 배율 (기본값 0.5f로 절반 축소) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float TargetMeshScale;

    // =========================================================================
    // 💡 [이속 장부 파괴 릭 완치 배관]: 중첩 디버프와 꼬이던 하드코딩 변수군들을 전량 소각하고,
    // 평타 및 상대방 CC 체계와 100% 동기화되는 GAS 순정 이동 속도 제어선으로 개통합니다.
    // =========================================================================
    /** 대시 전개 중 앨리스 본인의 이동 속도를 1.5배 가산 버프할 전용 대시 가속 이펙트 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice GAS")
    TSubclassOf<UGameplayEffect> DashSpeedGameplayEffectClass;

    /** 부착된 가속 이펙트를 종료/CC취소 시점에 무결하게 걷어내기 위한 GAS 활성 핸들 */
    FActiveGameplayEffectHandle DashSpeedActiveHandle;
};