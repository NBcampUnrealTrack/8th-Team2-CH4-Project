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
    // 미니언 개체별로 독립적인 상태 제어 및 스탯 관리를 위해 본체 직통 컴포넌트로 생성합니다.
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    
    // =========================================================================
    // [리뷰 저격 박멸 - 정석적인 AttributeSet 포인터 멤버 주입 완료]
    // 반환값을 버리지 않고 본체 장부(AttributeSet)에 명확히 기록 보관함으로써
    // 아키텍처의 의도를 명확히 하고 향후 C++ 단에서의 직접적인 스탯 접근성을 확보합니다.
    // =========================================================================
    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    // 멀티플레이어 환경에서의 정밀한 동기화를 위해 네트워크 복제 활성화
    SetReplicates(true);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 월드 진입과 동시에 미니언 핵심 전술 인프라 초기화 시동
    InitializeMinionInfrastructures();
}

void AFT_MinionCharacterBase::InitializeMinionInfrastructures()
{
    if (!AbilitySystemComponent) return;

    // 1. GAS 오너십 및 아바타 액터 관계선 바인딩 (순정 흐름 보장)
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    if (HasAuthority())
    {
        // =========================================================================
        // 부모 클래스의 가상 함수 의존성 없이, 본체 인프라가 개통되는 즉시 
        // 서버 주권 하에 고유 뇌세포(BrainAbilityClass)를 강제로 격발시켜 AI 틱을 깨웁니다.
        // =========================================================================
        if (BrainAbilityClass)
        {
            FGameplayAbilitySpec BrainSpec(BrainAbilityClass, 1);
            FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
            
            AbilitySystemComponent->TryActivateAbility(BrainHandle);
        }

        // =========================================================================
        // [AOS 피아식별 마스터 태그 낙인 개통]
        // 기획자가 배치하거나 스포너가 찔러준 팀 태그를 분석하여 루즈 태그로 상시 장착합니다.
        // =========================================================================
        if (MinionTeamTag.IsValid())
        {
            AbilitySystemComponent->AddLooseGameplayTag(MinionTeamTag);
        }

        // 2. 데이터 에셋 수치를 속성 장부에 원자적 이관
        if (MinionData)
        {
            // 데이터 에셋에 등록된 고유 외형 비주얼 메시 동적 매핑
            if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.Get())
            {
                GetMesh()->SetSkeletalMesh(TargetMesh);
            }

            // 기획 수치 기반 이동 속도 주입
            GetCharacterMovement()->MaxWalkSpeed = MinionData->DefaultMoveSpeed;

            // GAS 마스터 스탯 통장 동기화 기입 (기본 체력, 마스터 공격력, 이동 속도 반영)
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),   MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),      MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),   MinionData->DefaultMoveSpeed);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(), MinionData->DefaultAttackPower);

            // 데이터 에셋에 내장된 기본 어빌리티 목록(예: 평타 공격)을 가동 권한 장부에 순차 부여
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