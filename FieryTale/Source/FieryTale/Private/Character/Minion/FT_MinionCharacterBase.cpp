// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AFT_MinionCharacterBase::AFT_MinionCharacterBase()
{
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));

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

    // [서버 주권 결착 완착]: AI 컨트롤러가 소체를 완벽히 포제스한 시점에 명확히 닻을 내립니다.
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }
}

void AFT_MinionCharacterBase::OnRep_Controller()
{
    Super::OnRep_Controller();

    // [클라이언트 복제 안전망 개통]: 레이턴시 꼬임으로 인한 유령 액터 정보 등록을 완벽 방어하기 위해
    // 클라이언트 세션에 컨트롤러가 실재 안착했는지 정밀 검문 후 닻줄을 내립니다.
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

    // [비주얼 & 무브먼트 양단 동시 타설 레이어]
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
        // [이중 방어선 완착]: 서버 로컬 AI 컨트롤러가 즉시 팀을 검문할 수 있도록 LooseTag를 선행 주입하고,
        // 클라이언트 세션 전파를 위해 정석 GE 파이프라인을 병렬 가동하여 네트워킹 타이밍 릭을 원천 분쇄합니다.
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

        if (BrainAbilityClass)
        {
            FGameplayAbilitySpec BrainSpec(BrainAbilityClass, 1);
            FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
            
            AbilitySystemComponent->TryActivateAbility(BrainHandle);
        }
    }
}