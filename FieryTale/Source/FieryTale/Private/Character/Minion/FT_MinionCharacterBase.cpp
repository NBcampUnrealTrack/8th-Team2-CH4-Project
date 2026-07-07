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

    // 멀티플레이어 환경 무결성 확보: 미니언의 생명주기 및 위치 동기화를 세션 전역에 개통합니다.
    SetReplicates(true);
    ACharacter::SetReplicateMovement(true);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();
    
    // [수선 완료]: 지연 생성 무한 복제 아키텍처를 채택했으므로, 
    // 스포너가 세력 태그와 데이터 에셋 패킷을 완벽히 밀봉 조립해 줄 때까지 자동 시동을 보류합니다.
}

void AFT_MinionCharacterBase::LaunchMinionInfrastructure()
{
    // [심볼 미해결 완치]: 스포너가 모든 데이터 조립 완공을 선언한 바로 그 순간,
    // 본체의 전술 인프라 및 AI 뇌세포 깨우기 공정을 수동으로 정밀 가동합니다.
    InitializeMinionInfrastructures();
}

void AFT_MinionCharacterBase::InitializeMinionInfrastructures()
{
    if (!AbilitySystemComponent) return;

    // GAS 액터 정보 동기화: 서버와 클라이언트 양측 모두 본체와 ASC의 연결 고리를 완착합니다.
    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // =========================================================================
    // [비주얼 & 무브먼트 전원 개통 레벨]
    // 데이터 에셋 기반의 스켈레탈 메시 이식과 이속 동기화는 화면 연출을 위해 
    // 서버와 클라이언트 양측 전 영역에서 무조건 동시에 카운터 타설되어야 합니다.
    // =========================================================================
    if (MinionData)
    {
        // 데이터 에셋 내부 스켈레탈 메시 비주얼 강제 동기화 바인딩 (클라이언트 화면 완치)
        if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.LoadSynchronous())
        {
            GetMesh()->SetSkeletalMesh(TargetMesh);
        }

        // 무브먼트 컴포넌트 속도 동기화
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed = MinionData->DefaultMoveSpeed;
        }
    }

    // =========================================================================
    // [서버 주권 장부 주입 레벨]
    // 실질적인 게이지 데이터 기입, 태그 부여 및 어빌리티 사출 권한은 오직 서버에게만 있습니다.
    // =========================================================================
    if (HasAuthority())
    {
        // 1단계: AOS 피아식별 마스터 팀 태그 장착 (Blue / Red 진영 레이어 완착)
        // AI 컨트롤러의 센서 필터가 이 태그를 즉시 낚아채 피아식별 검문을 단행하게 됩니다.
        if (MinionTeamTag.IsValid())
        {
            AbilitySystemComponent->AddLooseGameplayTag(MinionTeamTag);
        }

        // 2단계: GAS 속성 장부에 데이터 에셋 깡수치 원자적 밀봉 기입
        if (MinionData)
        {
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

        // 3단계: 고유 뇌세포(BrainAbilityClass)를 격발시켜 AI 사고 회로 최종 가동
        // 팀 태그와 어빌리티 무장이 끝난 최적의 상태에서 사고 타이머를 격발합니다.
        if (BrainAbilityClass)
        {
            FGameplayAbilitySpec BrainSpec(BrainAbilityClass, 1);
            FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
            
            AbilitySystemComponent->TryActivateAbility(BrainHandle);
        }
    }
}