// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FT_UltimateGameplayAbility.h"
#include "FT_AliceUltimateAbility.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_AliceUltimateAbility : public UFT_UltimateGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFT_AliceUltimateAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 1초 뒤 앨리스 본인의 움직임을 봉쇄 해제하는 함수
	void ReleaseAlice(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo);

protected:
	// [기획 스펙] 자신 중심 시간 정지 영역 반지름 (예시: 6m)
	UPROPERTY(EditDefaultsOnly, Category = "Alice | Combat")
	float TimeStopRadius = 600.f;

	// [에셋 매핑] 적들에게 2초간 '기절(Stunned)'을 주입할 이펙트 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Alice | Effects")
	TSubclassOf<class UGameplayEffect> TimeStopDebuffEffectClass;

	// 앨리스 본인을 1초간 묶어둘 내부 타이머 핸들
	FTimerHandle AliceReleaseTimerHandle;
};
