// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FT_CharacterData.generated.h"

class UGameplayAbility;
class USkeletalMesh;
class UFT_WeaponData;

/**
 * FieryTale 영웅의 스켈레탈 메쉬, 스킬셋, 그리고 태생 초기 스탯을 규정하는 마스터 데이터 에셋입니다.
 */
UCLASS(BlueprintType, Blueprintable)
class FIERYTALE_API UFT_CharacterData : public UDataAsset
{
    GENERATED_BODY()

public:
    UFT_CharacterData();

    // --- 인라인 Getter 함수 파이프라인 ---
    FORCEINLINE class USkeletalMesh* GetCharacterMesh() const { return CharacterMesh; }
    FORCEINLINE const TMap<FGameplayTag, TSubclassOf<class UGameplayAbility>>& GetHeroAbilities() const { return CharacterAbilities; }
    FORCEINLINE class UFT_WeaponData* GetWeaponData() const { return WeaponData; }

    // 기본 스탯 자동화 주입을 위한 속성별 안전 Getter
    FORCEINLINE float GetDefaultMaxHealth() const { return DefaultMaxHealth; }
    FORCEINLINE float GetDefaultMaxShield() const { return DefaultMaxShield; }
    FORCEINLINE float GetDefaultMoveSpeed() const { return DefaultMoveSpeed; }
    FORCEINLINE float GetDefaultAttackPower() const { return DefaultAttackPower; }
    FORCEINLINE float GetDefaultMaxUltimateGauge() const { return DefaultMaxUltimateGauge; }

protected:
    // --- 외형 비주얼 데이터 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Visual")
    TObjectPtr<class USkeletalMesh> CharacterMesh;

    // --- 인풋 태그 기반 어빌리티 매핑 테이블 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Abilities")
    TMap<FGameplayTag, TSubclassOf<class UGameplayAbility>> CharacterAbilities;

    // --- 무기 메커니즘 데이터 에셋 연동 구역 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon")
    TObjectPtr<class UFT_WeaponData> WeaponData;
    
    // --- AttributeSet 초기화용 순정 초기 스탯 라인업 ---
    /** 캐릭터 최초 스폰 시 주입할 기본 최대 체력 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Default Attributes", meta = (ClampMin = "1.0"))
    float DefaultMaxHealth;

    /** 캐릭터 최초 스폰 시 주입할 기본 최대 보호막 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Default Attributes", meta = (ClampMin = "0.0"))
    float DefaultMaxShield;

    /** 캐릭터 최초 스폰 시 주입할 기본 이동 속도 (센티미터 단위, 예: 600.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Default Attributes", meta = (ClampMin = "0.0"))
    float DefaultMoveSpeed;

    /** 캐릭터 최초 스폰 시 주입할 기본 공격력 가중치 계수 (기본값: 1.0f) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Default Attributes", meta = (ClampMin = "0.0"))
    float DefaultAttackPower;

    /** 궁극기 활성화를 위해 채워야 하는 최대 궁극기 요구 게이지 수치 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Default Attributes", meta = (ClampMin = "1.0"))
    float DefaultMaxUltimateGauge;
};