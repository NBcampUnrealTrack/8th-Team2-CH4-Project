// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "FTSkillMetaData.generated.h"

class UGameplayAbility;
class UTexture2D;

/**
 * 스킬의 메타데이터를 정의하는 데이터 테이블 행 구조체입니다.
 */
USTRUCT(BlueprintType)
struct FFTSkillMetaData : public FTableRowBase
{
	GENERATED_BODY()

	/** 어빌리티 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Core")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** 스킬 표시 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Display")
	FText DisplayName;

	/** 스킬 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Display", meta = (MultiLine = true))
	FText Description;

	/** 특수 조작 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Display")
	FText HoldCastDescription;

	/** 스킬 아이콘 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 재사용 대기시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Design")
	float MaxCooldownTime = 0.0f;

	/** 쿨타임 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Tag")
	FGameplayTag CooldownTag;

	/** 스킬 분류 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Tag")
	FGameplayTag SkillCategoryTag;

	/** 궁극기 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Design")
	bool bIsUltimate = false;
};