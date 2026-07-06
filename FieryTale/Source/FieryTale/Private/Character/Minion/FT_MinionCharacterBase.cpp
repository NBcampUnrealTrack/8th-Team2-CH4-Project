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
    
    // [AttributeSet 명시적 포인터 바인딩]
    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    SetReplicates(true);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    
    //  [수선 완료]: 지연 생성 무한 복제 아키텍처를 채택했으므로, 
    // 스포너가 세력 태그와 데이터 에셋 패킷을 완벽히 밀봉 조립해 줄 때까지 자동 시동을 보류합니다.
}

void AFT_MinionCharacterBase::LaunchMinionInfrastructure()
{
    //  [심볼 미해결 완치]: 스포너가 모든 데이터 조립 완공을 선언한 바로 그 순간,
    // 본체의 전술 인프라 및 AI 뇌세포 깨우기 공정을 수동으로 정밀 가동합니다.
    InitializeMinionInfrastructures();
}

void AFT_MinionCharacterBase::InitializeMinionInfrastructures()
{
    if (!AbilitySystemComponent) return;

    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    if (HasAuthority())
    {
        // 1단계: 고유 뇌세포(BrainAbilityClass)를 격발시켜 AI 가동
        // 에디터 배치 창에서 직접 세팅했거나 스포너 단에서 낙인찍어준 뇌세포를 깨웁니다.
        if (BrainAbilityClass)
        {
            FGameplayAbilitySpec BrainSpec(BrainAbilityClass, 1);
            FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
            
            AbilitySystemComponent->TryActivateAbility(BrainHandle);
        }

        // 2단계: AOS 피아식별 마스터 팀 태그 장착 (Blue / Red 진영 레이어 완착)
        if (MinionTeamTag.IsValid())
        {
            AbilitySystemComponent->AddLooseGameplayTag(MinionTeamTag);
        }

        // 3단계: 스포너가 찔러준 데이터 에셋 수치 기반 비주얼 및 마스터 스탯 통장 원자적 기입
        if (MinionData)
        {
            // 데이터 에셋 내부 스켈레탈 메시 비주얼 강제 동기화 바인딩
            if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.Get())
            {
                GetMesh()->SetSkeletalMesh(TargetMesh);
            }

            // 무브먼트 컴포넌트 속도 동기화
            GetCharacterMovement()->MaxWalkSpeed = MinionData->DefaultMoveSpeed;

            // GAS 스탯 장부에 기획 깡수치 밀봉 기입
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),   MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),      MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),   MinionData->DefaultMoveSpeed);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(), MinionData->DefaultAttackPower);

            // 데이터 에셋 내부 평타 및 공격 어빌리티 일괄 주입
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