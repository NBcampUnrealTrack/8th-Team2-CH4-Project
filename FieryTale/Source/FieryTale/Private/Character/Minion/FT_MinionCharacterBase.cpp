// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Minion/FT_MinionCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Minion/DataAsset/FT_MinionData.h"
#include "Animation/AnimInstance.h"
#include "Character/Minion/FT_MinionAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/FTTags.h"
#include "Net/UnrealNetwork.h" 
#include "UObject/ConstructorHelpers.h"

AFT_MinionCharacterBase::AFT_MinionCharacterBase()
{
    // 1단계: ASC 및 AttributeSet 순정 컴포넌트 생성
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    UFT_AttributeSet* MutableAttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));
    AttributeSet = MutableAttributeSet;

    if (AbilitySystemComponent && MutableAttributeSet)
    {
        AbilitySystemComponent->AddSpawnedAttribute(MutableAttributeSet);
    }

    // [양손 무기 컴포넌트 독립 타설]
    MainWeaponComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainWeaponComponent"));
    if (MainWeaponComponent && GetMesh())
    {
        MainWeaponComponent->SetupAttachment(GetMesh());
        MainWeaponComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MainWeaponComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        MainWeaponComponent->SetIsReplicated(true); // 💡 넷코드 비주얼 동기화 선로 개통
    }

    SecondaryWeaponComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondaryWeaponComponent"));
    if (SecondaryWeaponComponent && GetMesh())
    {
        SecondaryWeaponComponent->SetupAttachment(GetMesh());
        SecondaryWeaponComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SecondaryWeaponComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        SecondaryWeaponComponent->SetIsReplicated(true); // 💡 넷코드 비주얼 동기화 선로 개통
    }

    // 무브먼트 리플리케이션 선로 개통
    ACharacter::SetReplicateMovement(true);

    AIControllerClass = AFT_MinionAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // AnimBP 미완성 대비 방어선: 기본 순정 애님 인스턴스 백업 장부 결속
    static ConstructorHelpers::FClassFinder<UAnimInstance> DefaultAnimInstanceFinder(TEXT("/Script/Engine.AnimInstance"));
    if (DefaultAnimInstanceFinder.Succeeded())
    {
        FallbackAnimClass = DefaultAnimInstanceFinder.Class;
    }
}

FTransform AFT_MinionCharacterBase::GetAttackLaunchTransform(FName CharacterFallbackSocketName) const
{
    // 💡 [무기 자체 소켓(FirePoint) 우선 추적 파이프라인]
    // 1. 주무기 컴포넌트 장부를 검문하여 실물 스태틱 메쉬 자산이 꽂혀있는지 징수합니다.
    if (MainWeaponComponent && MainWeaponComponent->GetStaticMesh())
    {
        // 2. 중요: 캐릭터 뼈대가 아니라, '스태틱 메쉬 에셋 자체'에 기획/아트 파트가 "FirePoint" 소켓을 심어두었는지 확인합니다.
        if (MainWeaponComponent->DoesSocketExist(TEXT("FirePoint")))
        {
            // 무기 끝단에 심어진 파이어포인트의 실시간 월드 트랜스폼을 정밀 사출하여 반환합니다!
            return MainWeaponComponent->GetSocketTransform(TEXT("FirePoint"), ERelativeTransformSpace::RTS_World);
        }
    }

    // 백업선 타설: 만약 무기 에셋 자체에 FirePoint 소켓이 없다면, 
    // 인자로 넘어온 캐릭터 소체 뼈대의 기본 소켓(Fallback) 주소지를 인양해 돌려주어 크래시를 차단합니다.
    if (GetMesh() && GetMesh()->DoesSocketExist(CharacterFallbackSocketName))
    {
        return GetMesh()->GetSocketTransform(CharacterFallbackSocketName, ERelativeTransformSpace::RTS_World);
    }

    // 최후의 마지노선: 둘 다 오염되어 없다면 시전자의 현재 가슴 높이 정면 트랜스폼을 안전장치로 리턴합니다.
    return FTransform(GetActorRotation(), GetActorLocation() + FVector(0.f, 0.f, 60.f));
}

void AFT_MinionCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AFT_MinionCharacterBase, MinionData);
    DOREPLIFETIME(AFT_MinionCharacterBase, MinionTeamTag);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();
}

void AFT_MinionCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);

        // [서버 권한]: Dead 태그 이벤트 바인딩 경비선 타설
        AbilitySystemComponent->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &AFT_MinionCharacterBase::OnDeadTagChanged);
    }
    
    if (HasAuthority())
    {
        // [2중 세이프티 가드]: 컨트롤러 누락 방어선
        if (GetController() == nullptr)
        {
            SpawnDefaultController();
        }
        
        // 서버 주권 하에 AI 결착이 완료된 이 순간 딱 1회만 인프라 초기화 기폭 (뇌 충돌 완치)
        InitializeMinionInfrastructures();
    }
}

void AFT_MinionCharacterBase::OnRep_MinionData()
{
    // 클라이언트 프록시는 데이터 에셋이 원격 복제 안착하는 이 시점에 하부 비주얼 인프라만 개통
    InitializeMinionInfrastructures();
}

void AFT_MinionCharacterBase::LaunchMinionInfrastructure()
{
    // 중복 기폭을 박멸하기 위해 PossessedBy에서 통제하므로 이 수동 트리거는 열어만 둡니다.
}

void AFT_MinionCharacterBase::InitializeMinionInfrastructures()
{
    if (!AbilitySystemComponent) return;

    AbilitySystemComponent->InitAbilityActorInfo(this, this);

    // [클라이언트 복제 프록시 세이프티 가드]: Minimal 모드용 태그 바인딩 안정선
    if (!HasAuthority())
    {
        AbilitySystemComponent->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &AFT_MinionCharacterBase::OnDeadTagChanged);
    }

    if (MinionData)
    {
        // 메쉬 및 애니메이션 동기화
        if (USkeletalMesh* TargetMesh = MinionData->MinionMesh.LoadSynchronous())
        {
            if (GetMesh())
            {
                GetMesh()->SetSkeletalMesh(TargetMesh);
                
                // AnimBP 누락 완전 진화 배관
                if (!MinionData->MinionAnimClass.IsNull())
                {
                    if (UClass* AnimBPClass = MinionData->MinionAnimClass.LoadSynchronous())
                    {
                        GetMesh()->SetAnimInstanceClass(AnimBPClass);
                    }
                }
                else if (FallbackAnimClass)
                {
                    GetMesh()->SetAnimInstanceClass(FallbackAnimClass);
                }

                // 💡 [컴포넌트 등록 및 메모리 안정성 가드선 타설]
                // 컴포넌트가 월드 장부에 등록되지 않은 임시 상태라면 무기 생성 전에 강제로 등록을 집행하여 
                // 소켓 조회 누수 및 널 포인터 크래시 리스크를 원천 봉쇄합니다.
                if (GetMesh()->IsRegistered() == false)
                {
                    GetMesh()->RegisterComponent();
                }

                // 💡 [1순위: 주무기 결착 레이어 기폭]
                if (MainWeaponComponent && !MinionData->MainWeaponMesh.IsNull())
                {
                    if (UStaticMesh* LoadedMainMesh = MinionData->MainWeaponMesh.LoadSynchronous())
                    {
                        MainWeaponComponent->SetStaticMesh(LoadedMainMesh);

                        // 💡 소켓 시스템이 완전히 로드되었는지 확인 후 스냅 결착
                        if (MinionData->MainWeaponSocketName != NAME_None && GetMesh()->DoesSocketExist(MinionData->MainWeaponSocketName))
                        {
                            if (MainWeaponComponent->IsRegistered() == false)
                            {
                                MainWeaponComponent->RegisterComponent();
                            }

                            MainWeaponComponent->AttachToComponent(
                                GetMesh(), 
                                FAttachmentTransformRules::SnapToTargetIncludingScale, 
                                MinionData->MainWeaponSocketName
                            );
                        }
                    }
                }

                // 💡 [2순위: 보조무기 결착 레이어 기폭 - 에셋에서 비워뒀다면 Null 스킵 처리]
                if (SecondaryWeaponComponent)
                {
                    if (!MinionData->SecondaryWeaponMesh.IsNull())
                    {
                        if (UStaticMesh* LoadedSecondaryMesh = MinionData->SecondaryWeaponMesh.LoadSynchronous())
                        {
                            SecondaryWeaponComponent->SetStaticMesh(LoadedSecondaryMesh);

                            if (MinionData->SecondaryWeaponSocketName != NAME_None && GetMesh()->DoesSocketExist(MinionData->SecondaryWeaponSocketName))
                            {
                                if (SecondaryWeaponComponent->IsRegistered() == false)
                                {
                                    SecondaryWeaponComponent->RegisterComponent();
                                }

                                SecondaryWeaponComponent->AttachToComponent(
                                    GetMesh(), 
                                    FAttachmentTransformRules::SnapToTargetIncludingScale, 
                                    MinionData->SecondaryWeaponSocketName
                                );
                            }
                        }
                    }
                    else
                    {
                        // 단검만 들거나 한손 무기 미니언일 경우 보조 메쉬 장부를 청결하게 포맷
                        SecondaryWeaponComponent->SetStaticMesh(nullptr);
                    }
                }
            }
        }

        // 이동 속도 기입 누락(0) 시 뇌사 방지 최소 속도 예외 보정
        float TargetMoveSpeed = MinionData->DefaultMoveSpeed;
        if (TargetMoveSpeed <= 0.0f)
        {
            TargetMoveSpeed = 400.0f;
        }

        if (GetCharacterMovement())
        {
            GetCharacterMovement()->MaxWalkSpeed = TargetMoveSpeed;
        }

        MinionProjectileClass = MinionData->ProjectileClass;
    }

    // [서버 주권 전용 단일 기폭 구역]: 중복 GiveAbility 연산을 완벽하게 멸균
    if (HasAuthority())
    {
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
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),   GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 400.f);
            AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(), MinionData->DefaultAttackPower);
            
            for (const TSubclassOf<UGameplayAbility>& AbilityClass : MinionData->MinionAbilities)
            {
                if (AbilityClass)
                {
                    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, -1, this));
                }
            }

            // [뇌사 뇌관 최종 박멸]: 데이터 에셋 내부의 진짜 뇌(MinionData->BrainAbilityClass)를 징수하여 확실하게 기폭
            if (MinionData->BrainAbilityClass)
            {
                FGameplayAbilitySpec BrainSpec(MinionData->BrainAbilityClass, 1);
                FGameplayAbilitySpecHandle BrainHandle = AbilitySystemComponent->GiveAbility(BrainSpec);
                
                AbilitySystemComponent->TryActivateAbility(BrainHandle);
            }
        }
    }
}

void AFT_MinionCharacterBase::OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
    if (NewCount > 0)
    {
        if (GetCapsuleComponent())
        {
            GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        }

        if (GetMesh())
        {
            GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            GetMesh()->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
        }

        // [양손 무기 사망 정산 소각]
        if (MainWeaponComponent)
        {
            MainWeaponComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            MainWeaponComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        }

        if (SecondaryWeaponComponent)
        {
            SecondaryWeaponComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SecondaryWeaponComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        }

        if (GetCharacterMovement())
        {
            GetCharacterMovement()->StopMovementImmediately();
            GetCharacterMovement()->DisableMovement();
        }
    }
}

void AFT_MinionCharacterBase::SetMinionDataAndTeam(UFT_MinionData* InMinionData, const FGameplayTag& InTeamTag)
{
    // 뇌 충돌 스팸 방어용 순수 장부 이식
    MinionData = InMinionData;
    MinionTeamTag = InTeamTag;
}   

void AFT_MinionCharacterBase::Die(AController* KillerController)
{
    Super::Die(KillerController);

    if (!AbilitySystemComponent) return;

    AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTStates::Core::Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
    
    if (AFT_MinionAIController* FTC = Cast<AFT_MinionAIController>(GetController()))
    {
        FTC->ClearFocus(EAIFocusPriority::Gameplay);
        FTC->StopMovement();
        
        FTC->ClearTargetEnemyActor(); 
    }

    if (HasAuthority())
    {
        SetLifeSpan(5.0f);
    }
}