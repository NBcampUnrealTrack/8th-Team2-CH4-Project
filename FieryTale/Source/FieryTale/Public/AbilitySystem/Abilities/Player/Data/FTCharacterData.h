// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Character/FTCharacterTypes.h"
#include "FTCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
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

	//	사망 시 재생할 몽타주 (소프트 참조 — 위 스켈레탈 메쉬와 동일한 스켈레톤 기준으로 제작되어야 함)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftObjectPtr<UAnimMontage> DeathMontage;

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

	//	LEquip 무기의 월드 스케일 배율. 무기는 항상 절대 크기(월드 스케일 1.0 × 이 값)로 부착되므로,
	//	원본 에셋 자체가 실제 크기 기준으로 제작되지 않은 경우(예: 거인 스켈레톤 기준 소품) 이 값으로 보정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon", meta = (ClampMin = "0.01"))
	float LEquipScale = 1.f;

	//	오른손(R_Hand 소켓)에 부착할 무기 스태틱 메쉬 (소프트 참조). 비어 있으면 오른손에 아무것도 붙지 않는다.
	//	이 메쉬에 "FirePoint" 소켓이 있으면, 평타 발사/판정 시작 위치가 캐릭터 기본 오프셋 대신 그 소켓 위치로 대체된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon")
	TSoftObjectPtr<UStaticMesh> REquip;

	//	REquip 무기의 월드 스케일 배율. LEquipScale과 동일한 용도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon", meta = (ClampMin = "0.01"))
	float REquipScale = 1.f;


	//  데이터 테이블의 이 행이 어떤 캐릭터 타입을 의미하는지 식별
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	EFTCharacterType CharacterType = EFTCharacterType::None;

	// 초상화 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftObjectPtr<UTexture2D> PortraitIcon;

	// --- 프리로딩 ---
	//	이 캐릭터가 게임에 존재할 때 아레나 시작 직후 미리 비동기 로딩해둘 에셋 목록.
	//	스킬/피격 이펙트(Niagara), 머티리얼, 사운드 등 "게임 중 확실히 쓰일" 무거운 에셋을 넣어두면
	//	최초 사용 시점의 로딩 히치를 방지한다. (타입 무관 — UObject 에셋이면 무엇이든 가능)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Preload")
	TArray<TSoftObjectPtr<UObject>> PreloadAssets;
};
