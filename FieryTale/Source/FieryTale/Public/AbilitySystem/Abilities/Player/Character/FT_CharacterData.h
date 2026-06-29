// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h" 
#include "FT_CharacterData.generated.h"

UCLASS()
class FIERYTALE_API UFT_CharacterData : public UDataAsset
{
	GENERATED_BODY()

public:
	FORCEINLINE class USkeletalMesh* GetCharacterMesh() const { return CharacterMesh; }
	FORCEINLINE const TMap<FGameplayTag, TSubclassOf<UGameplayAbility>>& GetHeroAbilities() const { return CharacterAbilities; }
    
	//  [선로 개통] 평타 스킬(GA)이나 캐릭터가 이 영웅의 무기 스펙을 동적으로 꺼내갈 수 있도록 Getter 추가!
	FORCEINLINE class UFT_WeaponData* GetWeaponData() const { return WeaponData; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> CharacterMesh;

	/** *  [팀원용 핵심 자동화 표] 
	 * 인풋 태그와 실행할 GA 자산을 1:1로 매핑합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TMap<FGameplayTag, TSubclassOf<UGameplayAbility>> CharacterAbilities;

	// ─────────────────────────────────────────────────────────────────────────
	// ⚔️ [🌟 무기 데이터 결합 마스터 구역]
	// 기획자가 에디터에서 해당 영웅에 맞는 무기 데이터 에셋(UFT_WeaponData)을 쏙 꽂아주는 칸입니다!
	// ─────────────────────────────────────────────────────────────────────────
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<class UFT_WeaponData> WeaponData;
};