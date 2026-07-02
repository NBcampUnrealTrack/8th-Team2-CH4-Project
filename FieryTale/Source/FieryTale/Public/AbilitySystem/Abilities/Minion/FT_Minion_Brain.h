// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "FT_Minion_Brain.generated.h"

class AAIController;
class AFTCharacterBase;
/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_Minion_Brain : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFT_Minion_Brain();

protected:
	/** 미니언 스폰 및 GAS 개통 즉시 시스템이 격발하는 순정 관문 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 미니언이 사망하거나 파괴될 때 안전하게 배관을 닫아줄 관문 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// =========================================================================
	// [기획 및 밸런스 제어 슬롯]
	// =========================================================================
	/** AI 전술 판단 및 시야 스캔을 돌릴 주기 (초 단위, 기본 0.5초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI | Config")
	float ScanInterval = 0.5f;

	/** 미니언 종류별 전방 시야 및 색적 반경 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI | Config")
	float ScanRange = 800.0f;

	/** 대상이 내 품안(공격 사거리)에 들어왔다고 판단할 마진선 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI | Config")
	float AttackAcceptanceRadius = 120.0f;

private:
	/** 심장 박동 타이머 주기에 맞춰 실시간 전술 판단을 집도하는 코어 엔진 함수 */
	void ExecuteAILogic();

	/** 주변 스캔 후 AOS 규칙에 따른 최적의 타겟(영웅 > 포탑 > 넥서스 > 미니언)을 반환 */
	AActor* InternalScanBestTarget(AFTCharacterBase* AvatarChar, const FGameplayTag& MyTeamTag);

	/** 메모리 누수 및 타이머 크래시 방지용 핸들 */
	FTimerHandle AIExecuteTimerHandle;
};
