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
 * FieryTale 라인 미니언들의 스탯, 외형 메쉬, 무기, GAS 스킬 명세를 
 * 기획 파트가 에디터에서 마스터 제어하도록 지원하는 데이터 에셋 클래스입니다.
 */
UCLASS(BlueprintType)
class FIERYTALE_API UFT_MinionData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Design")
    FGameplayTag MinionRoleTag;

    // =========================================================================
    // [미니언 비주얼 및 무기 사양 선언부]
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<USkeletalMesh> MinionMesh;

    /** 💡 [애니메이션 블루프린트 슬롯 타설]: 메쉬에 매칭되는 AnimBlueprint 클래스를 소프트 포인터로 인양합니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftClassPtr<UAnimInstance> MinionAnimClass;

    /** 💡 [주무기 자산 슬롯 (오른손 표준)]: 주로 오른손에 쥐여줄 주무기 메쉬 자산 주소입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<UStaticMesh> MainWeaponMesh;

    /** 💡 [주무기 결착 소켓]: 주무기가 장착될 소켓 이름입니다. (예: "Weapon_R_Socket") */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    FName MainWeaponSocketName;

    /** 💡 [보조무기 자산 슬롯 (왼손/방패 표준)]: 쌍검 미니언이나 방패병을 위해 왼손에 쥐여줄 보조무기 메쉬 자산 주소입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<UStaticMesh> SecondaryWeaponMesh;

    /** 💡 [보조무기 결착 소켓]: 보조무기가 장착될 소켓 이름입니다. (예: "Weapon_L_Socket") */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    FName SecondaryWeaponSocketName;

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
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS")
    TArray<TSubclassOf<UGameplayAbility>> MinionAbilities;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS", meta = (ToolTip = "원거리 미니언일 경우 사출할 투사체 블루프린트 클래스를 지정합니다."))
    TSubclassOf<AFT_ProjectileBase> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | AI")
    TSubclassOf<UGameplayAbility> BrainAbilityClass;
};