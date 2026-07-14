// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "FT_MinionData.generated.h"

class UGameplayAbility;
class USkeletalMesh;
class AFT_ProjectileBase;

/**
 * 미니언의 속성, 외형, 무기, 능력을 정의하는 데이터 에셋입니다.
 */
UCLASS(BlueprintType)
class FIERYTALE_API UFT_MinionData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Design")
    FGameplayTag MinionRoleTag;

    // =========================================================================
    // 시각 효과 및 장비
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<USkeletalMesh> MinionMesh;

    /** 애니메이션 블루프린트 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftClassPtr<UAnimInstance> MinionAnimClass;

    /** 주무기 (주로 오른손 장착) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<UStaticMesh> MainWeaponMesh;

    /** 주무기 장착 소켓 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    FName MainWeaponSocketName;

    /** 보조무기 (주로 왼손/방패 장착) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<UStaticMesh> SecondaryWeaponMesh;

    /** 보조무기 장착 소켓 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    FName SecondaryWeaponSocketName;

    // =========================================================================
    // 기본 속성
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultMaxHealth = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultMoveSpeed = 400.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultAttackPower = 15.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultAttackRange = 150.0f;

    // =========================================================================
    // 어빌리티 및 AI
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS")
    TArray<TSubclassOf<UGameplayAbility>> MinionAbilities;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS", meta = (ToolTip = "원거리 미니언일 경우 사출할 투사체 블루프린트 클래스를 지정합니다."))
    TSubclassOf<AFT_ProjectileBase> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | AI")
    TSubclassOf<UGameplayAbility> BrainAbilityClass;
};