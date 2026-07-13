// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "FT_MinionData.generated.h"

class UGameplayAbility;
class USkeletalMesh;
class AFT_ProjectileBase; // 💡 투사체 클래스 전방 선언 추가

/**
 * FieryTale 라인 미니언들의 스탯, 외형 메쉬, GAS 스킬 명세를 
 * 기획 파트가 에디터에서 마스터 제어하도록 지원하는 데이터 에셋 클래스입니다.
 */
UCLASS(BlueprintType)
class FIERYTALE_API UFT_MinionData : public UDataAsset
{
    GENERATED_BODY()

public:
    // =========================================================================
    // [미니언 식별 사양 선언부]
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Design")
    FGameplayTag MinionRoleTag;

    // =========================================================================
    // [미니언 비주얼 사양 선언부]
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<USkeletalMesh> MinionMesh;

    // =========================================================================
    // [미니언 기본 속성 수치 장부]
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultMaxHealth = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultMoveSpeed = 400.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Attributes")
    float DefaultAttackPower = 15.0f;

    // =========================================================================
    // [미니언 GAS 및 AI 인프라 자산 슬롯]
    // =========================================================================
    
    /** 미니언 전선 격돌 시 사출할 GAS 어빌리티 목록입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS")
    TArray<TSubclassOf<UGameplayAbility>> MinionAbilities;

    /** 💡 [원거리 투사체 배관 완착]: 기획자가 에디터에서 원거리 미니언용 투사체를 지정하는 슬롯입니다.
     * 근접 미니언일 경우 빈칸(nullptr)으로 두면 아까 구현한 근접 분기 코드가 알아서 처리합니다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS", meta = (ToolTip = "원거리 미니언일 경우 사출할 투사체 블루프린트 클래스를 지정합니다."))
    TSubclassOf<AFT_ProjectileBase> ProjectileClass;

    /** 미니언의 자율 사고 루프를 주도하는 마스터 브레인 어빌리티 클래스입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | AI")
    TSubclassOf<UGameplayAbility> BrainAbilityClass;
};