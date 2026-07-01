// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_AliceStealthDashSkill.generated.h"

class AFTPlayerCharacterBase;

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

private:
    // 무결점 보정 완료: 구현부에서 안전 복구 연산에 사용할 순정 최대 걷기 속도 백업 변수를 추가 락인합니다.
    /** 런타임 나누기 누적 오차로 인한 속도 증발을 막기 위한 순정 최대 속도 백업 보관소 */
    float OriginalMaxWalkSpeed;

    /** 런타임 캡슐 역연산 오차로 인한 무한 찌그러짐을 방지하기 위한 순정 반지름 백업 보관소 */
    float OriginalCapsuleRadius;

    /** 런타임 캡슐 축소 후 원래대로 되돌리기 위한 순정 높이 백업 보관소 */
    float OriginalCapsuleHalfHeight;
    
protected:
    /** 2초 버프 지속 시간이 끝났을 때 원래 크기와 속도로 되돌릴 안전 수거 콜백 함수 */
    UFUNCTION()
    void OnShrinkBuffFinished();

    /** 2초 유지를 칼같이 제어하여 런타임 누수를 차단할 버프 타이머 핸들 */
    FTimerHandle ShrinkBuffDurationTimerHandle;

    // --- 앨리스 신체 축소 고유 기획 스펙 데이터 블록 ---
    
    /** 몸이 작아진 동안 적용될 이동 속도 가속 배율 (기본값 1.5f 적용으로 50퍼센트 속도 증가 구현) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float MovementSpeedMultiplier;

    /** 축소 버프가 정상 유지되는 총 한계 시간 (기본값 2.0초 명세) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float MaxDuration;

    /** 히트박스 캡슐 콜리전 및 캐릭터 비주얼 메쉬 스케일을 동시에 줄이기 위한 목표 배율 (기본값 0.5f로 절반 축소) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Alice Spec")
    float TargetMeshScale;
};