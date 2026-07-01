// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FT_GameplayAbility.generated.h"

/** 
 * 프로젝트의 모든 게임플레이 어빌리티(GA)가 상속받을 베이스 클래스입니다.
 * 공통 변수나 유틸리티 함수를 이곳에 정의하여 관리 효율성을 높입니다.
 */
UCLASS()
class FIERYTALE_API UFT_GameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
    
public:
    
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    
	/** 
	 * 디버깅용 시각화 활성화 여부
	 * 에디터에서 체크하면 해당 스킬의 히트박스나 사거리 등을 화면에 그리는 로직에 활용할 수 있습니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Debug")
	bool bDrawDebugs = false;
    
private:
	UPROPERTY()
	FGameplayTagContainer RuntimeCooldownTags;
protected:

	// ◄◄◄ GAS 내부 규칙: 쿨타임 연산을 위해 아래 두 함수를 오버라이드합니다.
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 기획자가 에디터 디테일 패널에서 개별 지정할 자물쇠 쿨타임 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Cooldown")
	FGameplayTag CooldownTag;
};