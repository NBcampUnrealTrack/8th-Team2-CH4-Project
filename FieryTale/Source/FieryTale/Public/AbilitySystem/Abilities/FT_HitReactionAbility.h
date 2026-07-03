// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FT_GameplayAbility.h"
#include "FT_HitReactionAbility.generated.h"

class UAnimMontage;

/**
 * 전장의 모든 캐릭터 및 미니언들이 피격 이벤트를 수신했을 때
 * 애니메이션 경직 및 순간 제동을 집도하는 피격 반응 마스터 GA 클래스입니다.
 */
UCLASS()
class FIERYTALE_API UFT_HitReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFT_HitReactionAbility();

	/** 특정 태그 이벤트가 들어왔을 때 이 어빌리티가 자동으로 깨어나도록 처리하는 관문 오버라이드 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 경직 몽타주 종료 또는 중단 시 어빌리티를 청정하게 닫아줄 바인딩 함수 */
	UFUNCTION()
	void OnHitMontageCompletedOrInterrupted();

	// =========================================================================
	// [기획 및 비주얼 제어 슬롯]
	// =========================================================================
	/** 에디터 디테일 패널에서 피격 시 재생할 애니메이션 몽타주 주입 슬롯 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
	TObjectPtr<UAnimMontage> HitReactionMontage;
};