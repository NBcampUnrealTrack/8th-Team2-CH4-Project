// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "FT_AbilitySystemComponent.generated.h"

class UGameplayAbility;

/**
 * FieryTale 영웅 및 액터들의 게임플레이 어빌리티 제어 및 태그 관리를 총괄하는 마스터 커스텀 컴포넌트입니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FIERYTALE_API UFT_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// --- GAS 표준 내부 가상 함수 오버라이드 파이프라인 ---
	/** 서버 단에서 캐릭터에게 어빌리티 명세서(Spec)가 성공적으로 부여되었을 때 가동되는 통제 관문 */
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
    
	/** 클라이언트 네트워크 패킷이 도달하여 활성화된 어빌리티 목록이 복제 동기화될 때 가동되는 콜백 */
	virtual void OnRep_ActivateAbilities() override;
    
	// --- 기획 스펙 연동 실시간 레벨 제어 인터럽트 변환기 ---
	/** 런타임 인게임 도중 특정 어빌리티 클래스의 성장 레벨을 고정 수치로 다이렉트 강제 주입합니다 */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Abilities")
	void SetAbilityLevel(TSubclassOf<class UGameplayAbility> AbilityClass, int32 Level);
    
	/** 런타임 인게임 도중 레벨 성장에 따른 특정 스킬 레벨을 지정된 수치만큼 가산/진화시킵니다 */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Abilities")
	void AddToAbilityLevel(TSubclassOf<class UGameplayAbility> AbilityClass, int32 Level = 1);
    
private:
	/** 부여 시 즉시 가동되어야 하는 패시브 어빌리티 태그(ActivateOnGiven)를 선제 감시하여 가동시키는 내부 필터링 배관 */
	void HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec);
};