// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "FT_WeaponData.generated.h"

class UAnimMontage;

/**
 * 무기 공격 방식
 */
UENUM(BlueprintType)
enum class EWeaponFireType : uint8
{
    LineTrace       UMETA(DisplayName = "Line Trace (즉시 발사)"), // 빨간 망토
    Projectile      UMETA(DisplayName = "Projectile (투사체)"),   // 알라딘, 앨리스
    Melee           UMETA(DisplayName = "Melee (근접 판정)")       // 가구야 공주
};

/**
 * 무기 및 기본 공격의 특성을 정의하는 데이터 에셋입니다.
 */
UCLASS(BlueprintType, Blueprintable)
class FIERYTALE_API UFT_WeaponData : public UDataAsset
{
    GENERATED_BODY()
    
public:
    UFT_WeaponData();
    
    /** 공격 판정 방식 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Mechanism")
    EWeaponFireType FireType;

    // --- 기본 속성 ---
    /** 기본 피해량 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats")
    float BaseDamage;

    /** 공격 사거리 (또는 근접 공격 판정 범위) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats")
    float AttackRange;

    // --- 이동 제어 ---
    /** 공격 중 이동 불가 여부 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Movement")
    bool bRootOwnerDuringAttack;

    /** 공격 시 이동 속도 배율 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Movement")
    float MovementSpeedMultiplier;

    // --- 헤드샷 설정 (LineTrace 전용) ---
    /** 헤드샷 판정 활성화 여부 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Special", meta = (EditCondition = "FireType == EWeaponFireType::LineTrace"))
    bool bAllowHeadshot;

    /** 헤드샷 피해량 배율 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Special", meta = (EditCondition = "bAllowHeadshot"))
    float HeadshotMultiplier;

    // --- 탄퍼짐 설정 ---
    /** 기본 탄퍼짐 각도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Spread Data")
    float InitBaseSpread;

    /** 최대 탄퍼짐 각도 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Spread Data")
    float InitMaxSpread;

    // --- 투사체 설정 (Projectile 전용) ---
    /** 투사체 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Projectile Options", meta = (EditCondition = "FireType == EWeaponFireType::Projectile"))
    TSoftClassPtr<class AActor> ProjectileClass;

    /** 1회 공격 시 발사할 투사체 수 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Projectile Options", meta = (EditCondition = "FireType == EWeaponFireType::Projectile", ClampMin = "1"))
    int32 ProjectilesPerShot;

    /** 투사체 간 발사 지연 시간 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Projectile Options", meta = (EditCondition = "FireType == EWeaponFireType::Projectile && ProjectilesPerShot > 1"))
    float BurstDelay;

    /** 최대 도탄 횟수 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Projectile Options", meta = (EditCondition = "FireType == EWeaponFireType::Projectile"))
    int32 MaxBounceCount;

    // --- 시각 효과 ---
    /** 공격 애니메이션 몽타주 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Visuals")
    TSoftObjectPtr<class UAnimMontage> AttackMontage;

    /** 공격 적중 시 재생할 연출용 큐(GameplayCue) 태그입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Visuals")
    FGameplayTag HitGameplayCueTag;

public:
    /** 이동 속도 배율 반환 */
    FORCEINLINE float GetMovementPenaltyMultiplier() const 
    { 
        // 0 이하의 값 방지
        return MovementSpeedMultiplier > 0.0f ? MovementSpeedMultiplier : 1.0f; 
    }
};