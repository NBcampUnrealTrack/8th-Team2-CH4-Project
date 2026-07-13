// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AFT_MinionCharacterBase::AFT_MinionCharacterBase()
{
    // 대규모 군집 환경에서의 연산 및 네트워크 최적화를 위해 미니멀 리플리케이션 모드 적용
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

    // 멀티플레이 세션 가동을 위한 액터 및 무브먼트 복제(Replication) 활성화
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

    // 서버 권한: AI 컨트롤러가 소체를 포제스한 시점에 어빌리티 액터 인포 초기화 및 등록
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

void AFT_MinionCharacterBase::OnRep_Controller()
{
    Super::OnRep_Controller();

    // 클라이언트 권한: 복제 시차로 인한 오류를 방지하기 위해 컨트롤러 복제 안착 시점에 인포 초기화
    if (AbilitySystemComponent && GetController())
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        
        // 원격 클라이언트 사이드에서도 미니언의 외형 메쉬 및 기본 비주얼 인프라가 정상 동기화되도록 호출
        InitializeMinionInfrastructures();
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

    // [비주얼 및 무브먼트 데이터 초기화 레이어]
    if (MinionData)
    {
        // 소프트 object 포인터로 선언된 에셋을 동기 로드하여 메시 지정
        if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.LoadSynchronous())
        {
            if (GetMesh())
            {
                GetMesh()->SetSkeletalMesh(TargetMesh);
            }
        }

        // 데이터 에셋에 정의된 기본 이동 속도 반영
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed = MinionData->DefaultMoveSpeed;
        }

        // 원거리 공격 어빌리티가 실시간으로 참조할 수 있도록 데이터 에셋의 투사체 클래스를 소체에 캐싱
        MinionProjectileClass = MinionData->ProjectileClass;
    }

    // [서버 주권 전용 인프라 타설 레이어]
    if (HasAuthority())
    {
        // 서버 로컬 AI 컨트롤러의 즉각적인 진영 식별을 위한 루즈 태그 등록 및 원격지 동기화를 위한 GE 적용
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

        // 데이터 에셋 기반 기저 속성(Attributes) 수치 설정 및 스킬(GameplayAbility) 부여
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

        // 미니언의 자율 판단 및 행동 패턴을 제어하는 마스터 브레인 어빌리티 강제 격발
        if (BrainAbilityClass)
        {
            FGameplayAbilitySpec BrainSpec(BrainAbilityClass, 1);
            FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
            
            AbilitySystemComponent->TryActivateAbility(BrainHandle);
        }
    }
}