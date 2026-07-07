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
 *	스킬 1개의 어빌리티 + 표시 메타데이터를 한 행으로 묶는 DataTable Row.
 *	- 기획자가 DT_SkillMetaData 같은 데이터테이블에서 스킬별로 한 행씩 채운다.
 *	- HUD 스킬 위젯(UFTPlayerSkillInfoWidget 등)이 AbilityClass로 이 행을 조회해
 *	  아이콘/이름/설명/쿨다운을 표시하는 데 사용한다.
 *	- 슬롯(버튼) 배치 권한은 캐릭터 테이블(FFTCharacterData)이 가지므로 여기엔 InputTag를 두지 않는다.
 *	- 각 GA CDO에 흩어져 있던 표시 데이터를 이 테이블로 중앙 집중 관리하는 것이 목적이다.
 */
USTRUCT(BlueprintType)
struct FFTSkillMetaData : public FTableRowBase
{
	GENERATED_BODY()

	//	이 메타데이터가 연결되는 스킬 어빌리티 클래스. (테이블과 어빌리티를 묶는 핵심 키)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	//	[이관/폐기] 슬롯(버튼) 배치 권한을 캐릭터 테이블(FFTCharacterData)로 이관하면서 InputTag는 제거한다.
	//	폐기 정책상 삭제하지 않고 주석으로 보존 — 버튼→InputTag 매핑은 기존 태그를 소비 코드에서 재사용한다.
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skill")
	//FGameplayTag InputTag;

	//	스킬 표시 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skill")
	FText DisplayName;

	//	스킬 설명 (툴팁/상세창용, 여러 줄 허용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skill", meta = (MultiLine = true))
	FText Description;

	//	스킬 아이콘 머티리얼 — UImage::SetBrushFromMaterial로 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skill")
	TObjectPtr<UMaterialInterface> Icon;

	//	쿨다운 표시용 태그. 어빌리티(UFT_GameplayAbility)의 CooldownTag와 동일하게 맞춘다.
	//	(비워두면 쿨다운 바를 갱신하지 않는다. 궁극기처럼 게이지 기반이면 비워둔다.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale|Skill")
	FGameplayTag CooldownTag;
};
