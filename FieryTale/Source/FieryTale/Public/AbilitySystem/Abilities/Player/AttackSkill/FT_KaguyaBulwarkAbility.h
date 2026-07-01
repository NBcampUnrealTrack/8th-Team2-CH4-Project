// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_KaguyaBulwarkAbility.generated.h"

class UFT_WeaponData;
class AFTPlayerCharacterBase;

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
	/** 가구야 공주의 방벽 애니메이션 완료 및 마우스 릴리즈 중단 시 안전 이관을 통제할 내부 콜백 함수 */
	UFUNCTION()
	void OnGuardMontageFinished();

protected:
	/** 4초 가드 무한 버티기 악용 치트를 차단하고 타임아웃을 정밀 통제할 내부 지속 타이머 핸들 */
	FTimerHandle BulwarkDurationTimerHandle;

	// --- 가구야 봉래의 탄환 방벽 고유 기획 스펙 데이터 블록 ---
    
	/** 기획 스펙: 방벽 전개 중 가구야의 이동 속도 감산치 배율 (기본값 0.6f 대입으로 원래 속도의 60퍼센트만 유지하도록 조정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
	float MovementPenaltyMultiplier;

	/** 기획 스펙: 방벽을 최대로 유지할 수 있는 최대 지속 한계 시간 (기본값 4.0초 명세) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Kaguya Bulwark Spec")
	float MaxDuration;

private:
	/** 방벽 감산 페널티로 인한 무브먼트 나누기 누적 왜곡을 영구 방쇄하기 위한 순정 최대 속도 백업 보관소 */
	float OriginalMaxWalkSpeed;
};