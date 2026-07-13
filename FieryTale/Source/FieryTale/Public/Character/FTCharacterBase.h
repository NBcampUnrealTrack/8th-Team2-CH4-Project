// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "FTCharacterBase.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCharacterDiedDelegate, AFTCharacterBase*, DiedCharacter, AController*, KillerController);

UCLASS(Abstract)
class FIERYTALE_API AFTCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 서브클래스가 ASC 소스를 결정 (플레이어: PlayerState, AI: Self)
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return nullptr; }

	UPROPERTY(BlueprintAssignable, Category = "FieryTale | Character")
	FCharacterDiedDelegate OnCharacterDied;

	UFUNCTION(BlueprintCallable, Category = "FieryTale | Character")
	virtual void FinishDying();

	// 서버 권한 사망 진입점 — 체력 0 도달 시 AttributeSet에서 호출된다.
	// Dead 태그 부여로 중복 호출을 차단하고, 입력/이동 차단과 사망 델리게이트(리스폰 예약)를 처리한다.
	virtual void Die(AController* KillerController = nullptr);

	// GA_Death가 재생할 몽타주를 조회하기 위한 접근자 (영웅별 몽타주는 FFTCharacterData에서 스폰 시점에 주입됨).
	UFUNCTION(BlueprintPure, Category = "FieryTale | Animation")
	UAnimMontage* GetDeathMontage() const { return DeathMontage; }

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FieryTale | Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale | GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale | GAS")
	TArray<TSubclassOf<UGameplayEffect>> DefaultGameplayEffects;

	// PossessedBy에서 ASC를 확보한 뒤 이 함수로 어빌리티/이펙트를 일괄 등록
	// TODO:: 플레이어 캐릭터가 아닌 경우? GAS처리?
	// 필요없는 경우 삭제 예정
	void InitGAS(UAbilitySystemComponent* ASC);
};
