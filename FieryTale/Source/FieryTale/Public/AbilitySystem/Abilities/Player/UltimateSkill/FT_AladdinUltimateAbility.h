// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FT_UltimateGameplayAbility.h"
#include "FT_AladdinUltimateAbility.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFT_AladdinUltimateAbility : public UFT_UltimateGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFT_AladdinUltimateAbility();

protected:
	// 스킬 키가 눌릴 때마다 호출되는 GAS 기본 관문
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 지니가 전방을 강타하는 실질적인 물리 물리 스캔 함수
	void ExecuteGenieSmash(AActor* OwnerActor, UAbilitySystemComponent* SourceASC, int32 SmashIndex);

protected:
	// [추가완료] 에디터에서 GE_Damage 에셋을 꽂아주기 위한 클래스 참조 변수
	UPROPERTY(EditDefaultsOnly, Category = "Aladdin | Effects")
	TSubclassOf<class UGameplayEffect> DamageGameplayEffectClass;

	// [기획 스펙]直線 타격 범위 데이터 정의 (전방 15m)
	UPROPERTY(EditDefaultsOnly, Category = "Aladdin | Combat")
	float AttackRange = 1500.f; // 15미터

	UPROPERTY(EditDefaultsOnly, Category = "Aladdin | Combat")
	float AttackWidth = 300.f;  // 직선 박스의 좌우 반폭 (총 너비 600)

	UPROPERTY(EditDefaultsOnly, Category = "Aladdin | Combat")
	float BaseDamageValue = 50.f; // 기획서 반영 고정 피해 50
	// [상태 머신 트래커] 현재 몇 번째 소원인지 (1, 2, 3회)
	int32 CurrentWishCount = 0;
    
	// 최대 소원 개수
	const int32 MaxWishes = 3;
};
