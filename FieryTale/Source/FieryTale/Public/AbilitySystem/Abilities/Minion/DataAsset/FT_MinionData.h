// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "FT_MinionData.generated.h"

class UGameplayAbility;
class USkeletalMesh;

/**
 * FieryTale 라인 미니언(근접/원거리)들의 스탯, 외형 메쉬, GAS 스킬 명세를 
 * 기획 파트가 에디터에서 마스터 제어하도록 지원하는 데이터 에셋 클래스입니다.
 */
UCLASS(BlueprintType)
class FIERYTALE_API UFT_MinionData : public UDataAsset
{
	GENERATED_BODY()

public:
	// =========================================================================
	// [미니언 식별 사양 선언부]
	// 지령에 따라 에넘을 완전 배제하고, FTTags::FTMinionRole::Melee 또는 Ranged 태그를
	// 디테일 창에서 다이렉트로 주입받아 역할군을 명세합니다.
	// =========================================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Design")
	FGameplayTag MinionRoleTag;

	/** 미니언의 인게임 외형 비주얼을 담당할 에셋 슬롯 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | Visual")
	TObjectPtr<USkeletalMesh> MinionMesh;

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

	/** * 미니언이 전선 격돌 시 사출할 GAS 어빌리티 목록입니다.
	 * (예: 근접 미니언은 MeleeAttack GA, 원거리 미니언은 RangedProjectile 사출 GA)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion | GAS")
	TArray<TSubclassOf<UGameplayAbility>> MinionAbilities;
};