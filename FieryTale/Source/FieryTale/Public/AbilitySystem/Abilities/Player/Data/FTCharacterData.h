// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Character/FTCharacterTypes.h"
#include "FTCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UFT_WeaponData;

/**
 * 캐릭터 정보를 정의하는 데이터 테이블 행 구조체입니다.
 */
USTRUCT(BlueprintType)
struct FFTCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	// --- 기본 정보 ---
	// 캐릭터 표시 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	FText DisplayName;

	// --- 시각 효과 ---
	// 스켈레탈 메쉬
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	// 애니메이션 블루프린트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftClassPtr<UAnimInstance> AnimClass;

	// --- 스킬 설정 ---
	// 좌클릭 (기본 공격)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle LMBSkill;

	// 우클릭 (보조 스킬)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle RMBSkill;

	// 스페이스바 (이동/유틸리티 스킬)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle SpaceSkill;

	// R (궁극기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle RSkill;

	// --- 기본 스탯 ---
	// 기본 최대 체력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "1.0"))
	float DefaultMaxHealth = 100.f;

	// 기본 최대 보호막
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "0.0"))
	float DefaultMaxShield = 0.f;

	// 기본 이동 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "0.0"))
	float DefaultMoveSpeed = 600.f;

	// 기본 공격력 계수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "0.0"))
	float DefaultAttackPower = 1.f;

	// 최대 궁극기 게이지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "1.0"))
	float DefaultMaxUltimateGauge = 100.f;

	// --- 무기 데이터 ---
	// 무기 데이터 에셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon")
	TObjectPtr<UFT_WeaponData> WeaponData;
	
	
	// 캐릭터 타입
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	EFTCharacterType CharacterType;

	// 초상화 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftObjectPtr<UTexture2D> PortraitIcon;
};
