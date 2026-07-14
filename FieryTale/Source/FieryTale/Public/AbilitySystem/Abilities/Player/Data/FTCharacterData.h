// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Character/FTCharacterTypes.h"
#include "FTCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UFT_WeaponData;
class UStaticMesh;

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

	//	왼손(L_Hand 소켓)에 부착할 무기 스태틱 메쉬 (소프트 참조). 비어 있으면 왼손에 아무것도 붙지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon")
	TSoftObjectPtr<UStaticMesh> LEquip;

	//	오른손(R_Hand 소켓)에 부착할 무기 스태틱 메쉬 (소프트 참조). 비어 있으면 오른손에 아무것도 붙지 않는다.
	//	이 메쉬에 "FirePoint" 소켓이 있으면, 평타 발사/판정 시작 위치가 캐릭터 기본 오프셋 대신 그 소켓 위치로 대체된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon")
	TSoftObjectPtr<UStaticMesh> REquip;


	//  데이터 테이블의 이 행이 어떤 캐릭터 타입을 의미하는지 식별
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	EFTCharacterType CharacterType;

	// 초상화 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftObjectPtr<UTexture2D> PortraitIcon;
};
