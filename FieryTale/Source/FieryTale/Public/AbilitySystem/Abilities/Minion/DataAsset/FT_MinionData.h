// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "FT_MinionData.generated.h"

class UGameplayAbility;
class USkeletalMesh;

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
    // 에넘을 완전 배제하고, FTTags::FTMinionRole::Melee 또는 Ranged 태그를
    // 디테일 창에서 다이렉트로 주입받아 역할군을 명세합니다.
    // =========================================================================
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Design")
    FGameplayTag MinionRoleTag;

    /** 
     * [메모리 릭 수선 완료]: 하드 레퍼런스를 소각하고 소프트 레퍼런스로 전환합니다.
     * 게임 기동 시 무거운 비주얼 데이터가 메모리에 상시 잔주하는 현상을 차단하고, 
     * 스포너가 지연 생성을 단행하는 런타임 순간에만 스트리밍 로드되도록 제어합니다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
    TSoftObjectPtr<USkeletalMesh> MinionMesh;

    // =========================================================================
    // [미니언 기본 속성 수치 장부]
    // 미니언이 스폰될 때 AttributeSet으로 안전하게 이관될 기저 스탯들입니다.
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
    
    /** 미니언 전선 격돌 시 사출할 GAS 어빌리티 목록입니다.
     * 근접 미니언은 MeleeAttack GA, 원거리 미니언은 RangedProjectile 사출 GA를 매핑합니다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS")
    TArray<TSubclassOf<UGameplayAbility>> MinionAbilities;

    /** 
     * [네이밍 가드 완착]: 본체 캐릭터의 런타임 변수와 혼동되지 않도록 명 명세를 정비했습니다.
     * 에디터 단에서 기획자가 장착해 둔 기저 사고 회로의 원본 클래스 명세입니다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | AI")
    TSubclassOf<UGameplayAbility> DefaultBrainAbilityClass;
};