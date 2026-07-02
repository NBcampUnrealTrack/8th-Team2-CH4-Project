// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AFT_MinionCharacterBase::AFT_MinionCharacterBase()
{
    // [GAS 내장형 아키텍처 완착]
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    SetReplicates(true);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 월드 진입 시 인프라 초기화 진입
    InitializeMinionInfrastructures();
}

void AFT_MinionCharacterBase::InitializeMinionInfrastructures()
{
    if (!AbilitySystemComponent) return;

    // 1. GAS 오너십 및 아바타 액터 관계선 바인딩
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    if (HasAuthority())
    {
        // =========================================================================
        // [AOS 피아식별 마스터 태그 낙인 개통 완착]
        // 강제 형변환(static_cast) 및 존재하지 않는 Hub 멤버선을 완전히 도려내고,
        // 순정 API 규칙에 맞춰 깔끔하게 단일 인자로 태그만 누적 처리합니다.
        // =========================================================================
        if (MinionTeamTag.IsValid())
        {
            AbilitySystemComponent->AddLooseGameplayTag(MinionTeamTag);
        }

        // 2. 데이터 에셋 수치를 속성 장부에 원자적 이관
        if (MinionData)
        {
            // [비주얼 매핑 수선] TObjectPtr 구조에 맞게 순정 .Get() 혹은 생포인터 캐스팅선으로 안전 가드합니다.
            if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.Get())
            {
                GetMesh()->SetSkeletalMesh(TargetMesh);
            }

            GetCharacterMovement()->MaxWalkSpeed = MinionData->DefaultMoveSpeed;

            // GAS 마스터 스탯 통장 동기화 기입
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),   MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),      MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),   MinionData->DefaultMoveSpeed);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(), MinionData->DefaultAttackPower);

            // 인풋이 없는 미니언 전용 기동 스펙 허가(GiveAbility)
            for (const TSubclassOf<UGameplayAbility>& AbilityClass : MinionData->MinionAbilities)
            {
                if (AbilityClass)
                {
                    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, -1, this));
                }
            }
        }
    }
}