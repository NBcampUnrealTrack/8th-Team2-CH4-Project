// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Character/FTCharacterBase.h"
#include "FT_MinionCharacterBase.generated.h"

class UAbilitySystemComponent;
class UFT_MinionData;
class UGameplayAbility;

/**
 * FieryTale 전장의 블루 vs 레드 진영 미니언들을 총괄하는 GAS 내장형 C++ 마스터 캐릭터 클래스입니다.
 */
UCLASS()
class FIERYTALE_API AFT_MinionCharacterBase : public AFTCharacterBase
{
    GENERATED_BODY()

public:
    AFT_MinionCharacterBase();

    /** [IAbilitySystemInterface 순정 오버라이드] 미니언 본체 고유의 내장형 ASC 주소지를 사출합니다. */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

protected:
    /** 월드 진입 즉시 서버 권한 하에 스탯 초기화 및 인프라 구축을 집도하는 관문 */
    virtual void BeginPlay() override;

    /** 미니언 본체에 직통 내장되어 모든 전술 상태 관리 및 어빌리티 격발을 통제할 고유 GAS 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** [리뷰 저격 박멸 - 정석 스탯 장부 보관 슬롯]
     * 생성자에서 공중에 버려지던 AttributeSet 객체를 본체 변수에 안전하게 명시적 바인딩합니다.
     * 이를 통해 아키텍처 의도를 명확히 하고, 후속 C++ 코드 단에서 미니언의 실시간 스탯 직접 참조 권한을 확보합니다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | GAS")
    TObjectPtr<const class UFT_AttributeSet> AttributeSet;

    /** 기획자가 월드 배치 창(디테일 패널)이나 스폰 매니저에서 낙인찍어줄 미니언의 AOS 팀 세력 태그 
     * (예: FTTags::FTFaction::Team_Blue 혹은 FTTags::FTFaction::Team_Red)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    FGameplayTag MinionTeamTag;

    /** 기획자가 빌드한 미니언 전용 데이터 에셋 (외형 메시, 기본 체력, 이동 속도, 공격 스펙 슬롯) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TObjectPtr<UFT_MinionData> MinionData;

    /** 에디터 디테일 패널에서 C++ 고유 AI 뇌세포인 'UFT_Minion_Brain' 클래스를 정밀 매핑해 주는 주입구입니다.
     * 스폰과 동시에 서버 주권 하에 해당 어빌리티를 강제로 격발시켜 AI 틱을 깨웁니다. 
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TSubclassOf<UGameplayAbility> BrainAbilityClass;

private:
    /** 주입된 데이터 에셋 수치 동기화, 피아식별 태그 낙인, AI 브레인 시동을 원자적으로 통합 처리하는 공정 함수 */
    void InitializeMinionInfrastructures();
    
};