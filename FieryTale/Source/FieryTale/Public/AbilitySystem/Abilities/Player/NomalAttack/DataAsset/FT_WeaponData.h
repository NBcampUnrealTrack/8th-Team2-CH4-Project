// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FT_WeaponData.generated.h"

class UAnimMontage;

/**
 * 무기 격발 메커니즘 타입 열거형
 */
UENUM(BlueprintType)
enum class EWeaponFireType : uint8
{
	LineTrace       UMETA(DisplayName = "Line Trace (즉시 발사)"), // 빨간 망토
	Projectile      UMETA(DisplayName = "Projectile (투사체)"),   // 알라딘, 앨리스
	Melee           UMETA(DisplayName = "Melee (근접 판정)")       // 가구야 공주
};

UCLASS(BlueprintType, Blueprintable)
class FIERYTALE_API UFT_WeaponData : public UDataAsset
{
	GENERATED_BODY()
public:
	
	UFT_WeaponData();
	
	/** 어떤 방식으로 공격 판정을 처리할 것인가 (근접, 투사체 ,라인트레이스 )*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Mechanism")
	EWeaponFireType FireType;

	// --- 기본 전투 스탯 ---
	/** 평타 기본 바디샷/적중 대미지 수치 (기획 기준: 30) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Stats")
	float BaseDamage;

	/** 평타 사격 사거리 혹은 근접 히트박스 길이 (장총 컨셉 등 적용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Stats")
	float AttackRange;

	// --- 패널티 및 이동 속도 제어 데이터 ---
	/** 평타 격발(채널링) 도중 시전자를 완전히 제자리에 묶을 것인가 (알라딘 0.5초 고정용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Movement")
	bool bRootOwnerDuringAttack;

	/** 공격 혹은 정조준 시 적용할 이동 속도 승수 배율 (빨간 망토 조준 시 0.7f, 일반 1.0f) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Movement")
	float MovementSpeedMultiplier;

	// --- 정조준 및 헤드샷 판정 특화 (빨간 망토) ---
	/** 정조준 및 헤드 본(Bone) 충돌체 검사를 허용할 것인가 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Special")
	bool bAllowHeadshot;

	/** 헤드샷 성공 시 가산할 대미지 배율 (기획 기준 기본 2.0배 -> 30을 60으로 변환) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Special")
	float HeadshotMultiplier;

	// --- 투사체 사격 세부 옵션 (알라딘 연사 / 앨리스 도탄 및 멀티샷) ---
	/** 1회 공격 인풋당 동시 또는 연속으로 사출될 투사체 총 개수 (알라딘 기류 탄환은 3개) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Projectile Options", meta = (ClampMin = "1"))
	int32 ProjectilesPerShot;

	/** 연사(Burst) 형태로 투사체가 나갈 때 탄환 간의 시간 간격 딜레이 (알라딘용 3연사 타이머용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Projectile Options", meta = (EditCondition = "ProjectilesPerShot > 1"))
	float BurstDelay;

	/** 지형이나 벽 충돌 시 최대 튕김/도탄을 허용할 횟수 (앨리스 로얄 카드는 1회) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Projectile Options")
	int32 MaxBounceCount;

	// --- 시각 에셋 바인딩 ---
	/** 평타 격발 시 캐릭터가 재생할 전용 애니메이션 몽타주 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FT|Visuals")
	TObjectPtr<UAnimMontage> AttackMontage;
};