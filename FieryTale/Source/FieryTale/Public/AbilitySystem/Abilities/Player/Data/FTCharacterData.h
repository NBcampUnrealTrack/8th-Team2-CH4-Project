// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FTCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UFT_WeaponData;

/**
 *	캐릭터(영웅) 1명의 정의를 한 행으로 담는 DataTable Row.
 *	- 기획자가 DT_CharacterData 같은 데이터테이블에서 영웅별로 한 행씩 채운다.
 *	- 스킬 슬롯(LMB/RMB/Space/R)은 FFTSkillMetaData 테이블(DT_SkillMetaData)의 행을 골라 연결한다.
 *	- 메쉬/애님BP는 소프트 참조라, 영웅 선택 시점에만 로드된다.(로스터 전체가 메모리에 올라오지 않게)
 *	  소비 측(캐릭터 스폰 로직)에서 LoadSynchronous 또는 비동기 스트리밍으로 해제해서 사용할 것.
 *
 *	NOTE: 기존 UFT_CharacterData(DataAsset)를 완전 대체하는 리팩토링 축.
 *	      (외형 + 스킬 슬롯 + 초기 스탯 + 무기 데이터를 모두 포함)
 */
USTRUCT(BlueprintType)
struct FFTCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	// --- 정체성 ---
	//	캐릭터(영웅) 표시 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	FText DisplayName;

	// --- 외형 ---
	//	캐릭터 스켈레탈 메쉬 (소프트 참조 — 선택 시점에만 로드)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	//	위 메쉬에 적용할 애니메이션 블루프린트 클래스 (소프트 참조)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Character")
	TSoftClassPtr<UAnimInstance> AnimClass;

	// --- 스킬 슬롯: 각 버튼에 매핑될 스킬을 SkillMetaData 테이블에서 선택 ---
	//	RowType 메타로 드롭다운을 FFTSkillMetaData 행으로 필터링한다.
	//	LMB (좌클릭, 보통 일반 공격)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle LMBSkill;

	//	RMB (우클릭, 보조 공격 스킬)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle RMBSkill;

	//	Space (이동/유틸 스킬)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle SpaceSkill;

	//	R (궁극기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skills", meta = (RowType = "FTSkillMetaData"))
	FDataTableRowHandle RSkill;

	// --- AttributeSet 초기화용 기본 스탯 (구 UFT_CharacterData에서 이관) ---
	//	최초 스폰/리스폰 시 주입할 기본 최대 체력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "1.0"))
	float DefaultMaxHealth = 100.f;

	//	기본 최대 보호막
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "0.0"))
	float DefaultMaxShield = 0.f;

	//	기본 이동 속도 (cm/s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "0.0"))
	float DefaultMoveSpeed = 600.f;

	//	기본 공격력 가중치 계수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "0.0"))
	float DefaultAttackPower = 1.f;

	//	궁극기 활성화에 필요한 최대 게이지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Stats", meta = (ClampMin = "1.0"))
	float DefaultMaxUltimateGauge = 100.f;

	// --- 무기 데이터 (구 UFT_CharacterData에서 이관) ---
	//	평타/사격 메커니즘 설정 에셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Weapon")
	TObjectPtr<UFT_WeaponData> WeaponData;
};
