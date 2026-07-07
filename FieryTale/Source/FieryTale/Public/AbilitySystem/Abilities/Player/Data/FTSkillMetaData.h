// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "FTSkillMetaData.generated.h"

class UGameplayAbility;
class UMaterialInterface;

/**
 * 스킬 1개의 어빌리티 및 표시 메타데이터를 한 행으로 묶는 DataTable Row 구조체입니다.
 */
USTRUCT(BlueprintType)
struct FFTSkillMetaData : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 메타데이터와 결합되어 인게임 실물 가동을 집도할 GAS 어빌리티 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Core")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** 스킬창이나 HUD 툴팁에 실시간 출력될 스킬의 고유 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Display")
	FText DisplayName;

	/** 스킬의 상세 효과 명세 설명 문구 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Display", meta = (MultiLine = true))
	FText Description;

	/** 차징 조작이나 채널링 유지 시간 등 특수 조작법을 출력할 가이드 텍스트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Display")
	FText HoldCastDescription;

	/** 스킬 슬롯에 표현될 2D 그래픽 머티리얼 주소 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
	TObjectPtr<UMaterialInterface> Icon;

	/** 이 스킬의 기획서 고유 재사용 대기시간 원본 수치 (UI 쿨다운 바 똬리 연산용 원자 데이터) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Design")
	float MaxCooldownTime = 0.0f;

	/** 이 스킬이 돌 때 가동될 고유 쿨타임 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Tag")
	FGameplayTag CooldownTag;

	/** 스킬 분류 장부에 등록될 마스터 분류 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Tag")
	FGameplayTag SkillCategoryTag;

	/** HUD 스킬 위젯에서 특수 연출 테두리를 인지하기 위한 궁극기 판정 플래그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Design")
	bool bIsUltimate = false;
};