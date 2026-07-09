// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AFT_MinionCharacterBase::AFT_MinionCharacterBase()
{
    // 미니언은 대규모 군집 연산 최적화를 위해 미니멀 리플리케이션 모드를 확정 적용합니다.
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    // 멀티플레이 세션 가동을 위한 순정 네트워킹 리플리케이션 명세를 결착합니다.
    SetReplicates(true);
    ACharacter::SetReplicateMovement(true);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();
}

void AFT_MinionCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // [서버 주권 결착 완착]: AI 컨트롤러가 소체를 완전히 포제스한 시점에 닻을 내리고 명확히 인프라를 등록합니다.
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

void AFT_MinionCharacterBase::OnRep_Controller()
{
    Super::OnRep_Controller();

    // [클라이언트 복제 안전망 개통]: 레이턴시 꼬임으로 인한 유령 액터 정보 등록 결함을 완벽히 방어합니다.
    // 클라이언트 세션에 컨트롤러 복제가 완전 안착했는지 정밀 검문 후 어빌리티 액터 인포 지지대를 구축합니다.
    if (AbilitySystemComponent && GetController())
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

void AFT_MinionCharacterBase::LaunchMinionInfrastructure()
{
    InitializeMinionInfrastructures();
}

void AFT_MinionCharacterBase::InitializeMinionInfrastructures()
{
    if (!AbilitySystemComponent) return;

    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // [비주얼 and 무브먼트 양단 동시 타설 레이어]
    if (MinionData)
    {
        if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.LoadSynchronous())
        {
            GetMesh()->SetSkeletalMesh(TargetMesh);
        }

        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed = MinionData->DefaultMoveSpeed;
        }
    }

    // [서버 전용 권한 장부 확정 라인]
    if (HasAuthority())
    {
        // [이중 방어선 완착]: 서버 로컬 AI 컨트롤러가 지체 없이 팀 진영을 식별하도록 룹태그를 먼저 사출하고,
        // 클라이언트 원격 동기화를 위해 정석 GE 파이프라인을 연동시켜 네트워킹 시차 렉을 전면 분쇄합니다.
        if (MinionTeamTag.IsValid())
        {
            AbilitySystemComponent->AddLooseGameplayTag(MinionTeamTag);

            if (TeamGameplayEffectClass)
            {
                FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
                EffectContext.AddSourceObject(this);
                FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(TeamGameplayEffectClass, 1.0f, EffectContext);
                if (SpecHandle.IsValid())
                {
                    AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                }
            }
        }

        // 데이터 에셋 기반의 기저 속성 연산 장부를 할당하고 전술 스킬 무장을 기부합니다.
        if (MinionData)
        {
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),   MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),      MinionData->DefaultMaxHealth);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),   MinionData->DefaultMoveSpeed);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(), MinionData->DefaultAttackPower);
            
            for (const TSubclassOf<UGameplayAbility>& AbilityClass : MinionData->MinionAbilities)
            {
                if (AbilityClass)
                {
                    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, -1, this));
                }
            }
        }

        // 미니언의 전술 사고 회로이자 행동 패턴을 주도하는 마스터 브레인 어빌리티를 강제 격발합니다.
        if (BrainAbilityClass)
        {
            FGameplayAbilitySpec BrainSpec(BrainAbilityClass, 1);
            FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
            
            AbilitySystemComponent->TryActivateAbility(BrainHandle);
        }
    }
}