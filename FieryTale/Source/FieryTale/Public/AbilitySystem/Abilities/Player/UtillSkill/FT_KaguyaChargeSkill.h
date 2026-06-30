// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FT_GameplayAbility.h"
#include "FT_KaguyaChargeSkill.generated.h"

class AFTPlayerCharacterBase;

/**
 * 가구야 공주 Shift 기술 - 대나무 숲의 숨결 어빌리티 시스템
 */
UCLASS()
class FIERYTALE_API UFT_KaguyaChargeSkill : public UFT_GameplayAbility
{
	GENERATED_BODY()

public:
	UFT_KaguyaChargeSkill();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 연막 영역 지속 시간이 완료되었을 때 호출될 콜백 함수
	UFUNCTION()
	void OnChargeFinished();

	// 4초 유지 스펙을 정확하게 제어할 순정 타이머 핸들 변수
	FTimerHandle BambooGroveDurationTimerHandle;

	// 기획 스펙: 전술 연막 안개 영역 생성 반경 (450.0f)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Area")
	float BambooGroveRadius;

	// 기획 스펙: 연막 영역 총 유지 시간 (4.0초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Area")
	float BambooGroveDuration;

	// 연막 내부 아군에게 주입할 은신(Invisibility) 관련 GameplayEffect 에셋 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Effect")
	TSubclassOf<UGameplayEffect> ConcealmentEffectClass;
};