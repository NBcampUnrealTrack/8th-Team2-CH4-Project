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
#include "Object/FT_MinionSpawner.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Abilities/GameplayAbility.h" // UGameplayAbility 불완전 타입(Incomplete type) 컴파일 에러 방지

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

    // 평타(히트스캔) 등 라인트레이스(Visibility 채널)에 맞기 위해 콜리전 설정
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel1, ECollisionResponse::ECR_Block);
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

    // 💡 부드럽게 이동 방향을 바라보도록 회전 동기화 설정
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = true;
        MoveComp->bUseControllerDesiredRotation = true; // 정지 후 공격 시 AI 포커스(적) 방향으로 자연스럽게 회전
        MoveComp->RotationRate = FRotator(0.0f, 600.0f, 0.0f); // 부드러운 회전 속도 (Yaw)
    }

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
    DOREPLIFETIME(AFT_MinionCharacterBase, bIsActiveInPool);
}

void AFT_MinionCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    // 블루프린트(CDO)에서 Pawn 프로필 기본값으로 덮어씌워져 Visibility가 무시되는 현상 방지
    // 새로 스폰된 미니언도 레드후드(플레이어)의 라인트레이스(ECC_Visibility)에 무조건 맞도록 강제합니다.
    if (GetCapsuleComponent())
    {
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel1, ECollisionResponse::ECR_Block);
    }
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

                // 팀 색상(피아식별) 머티리얼 적용 — 반드시 SetSkeletalMesh 이후여야 새 메쉬의 머티리얼이 초기화되지 않는다.
                // 이 함수(InitializeMinionInfrastructures)는 서버(PossessedBy)·클라이언트(OnRep_MinionData) 양단에서 실행되므로
                // 모든 화면에서 팀 색상이 동일하게 반영된다.
                ApplyTeamColorOverride();

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

    //	덮어쓰기 머티리얼이 있을 경우 MID를 생성하고 팀 색상 변수(isRedTeam)에 태그에 맞는 값을 주입한다.
    //	단, 실제 적용은 여기가 아니라 ApplyTeamColorOverride()에서 한다:
    //	 - 이 함수는 스포너의 Deferred 스폰 직후(서버 전용) 호출되어 스켈레탈 메쉬가 아직 확정되지 않았고,
    //	   이후 InitializeMinionInfrastructures의 SetSkeletalMesh가 머티리얼을 초기화하므로 여기서 걸어도 덮어써진다.
    //	 - 또한 이 함수는 서버에서만 실행되어 클라이언트에는 팀 색상이 반영되지 않는다.
    //	따라서 메쉬가 확정되고 서버·클라 양단에서 실행되는 InitializeMinionInfrastructures()에서 처리한다.
}

void AFT_MinionCharacterBase::ApplyTeamColorOverride()
{
    //	덮어쓰기 머티리얼이 지정된 경우에만 팀 색상 MID를 적용한다(없으면 메쉬 원본 머티리얼을 그대로 유지).
    if (!MinionData || MinionData->MinionMaterialOverride.IsNull() || !GetMesh())
    {
        return;
    }

    UMaterialInterface* BaseOverride = MinionData->MinionMaterialOverride.LoadSynchronous();
    if (!BaseOverride)
    {
        return;
    }

    //	팀 태그로 색상 스칼라 값을 결정한다 — 레드=1, 그 외(블루/미지정)=0.
    const bool bIsRedTeam = (MinionTeamTag == FTTags::FTFaction::Team_Red);

    //	덮어쓰기 머티리얼로 MID를 생성하고 팀 색상 변수(isRedTeam)를 주입한다.
    UMaterialInstanceDynamic* TeamMID = UMaterialInstanceDynamic::Create(BaseOverride, this);
    if (!TeamMID)
    {
        return;
    }
    TeamMID->SetScalarParameterValue(TEXT("isRedTeam"), bIsRedTeam ? 1.0f : 0.0f);

    //	피아식별용 단일 오버라이드이므로 메쉬의 모든 머티리얼 슬롯에 같은 MID를 덮어씌운다.
    const int32 NumMaterials = GetMesh()->GetNumMaterials();
    for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
    {
        GetMesh()->SetMaterial(SlotIndex, TeamMID);
    }

    //	적용된 MID를 본체 멤버에 캐싱한다(이후 참조/재조정용).
    MinionMaterialOverride = TeamMID;
}

void AFT_MinionCharacterBase::Die(AController* KillerController)
{
    Super::Die(KillerController);

    if (!AbilitySystemComponent) return;

    
    if (AFT_MinionAIController* FTC = Cast<AFT_MinionAIController>(GetController()))
    {
        FTC->ClearFocus(EAIFocusPriority::Gameplay);
        FTC->StopMovement();
        
        FTC->ClearTargetEnemyActor(); 
    }

    if (HasAuthority())
    {
        // 5초 뒤 오브젝트 풀로 반환
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(PoolReturnTimerHandle, this, &AFT_MinionCharacterBase::DeactivateForPool, 5.0f, false);
        }
    }
}

void AFT_MinionCharacterBase::DeactivateForPool()
{
    if (!HasAuthority()) return;

    bIsActiveInPool = false;
    OnRep_IsActiveInPool();

    if (OwningSpawner)
    {
        OwningSpawner->ReturnMinionToPool(this);
    }
}

void AFT_MinionCharacterBase::OnRep_IsActiveInPool()
{
    SetActorHiddenInGame(!bIsActiveInPool);
    SetActorEnableCollision(bIsActiveInPool);
    SetActorTickEnabled(bIsActiveInPool);

    if (bIsActiveInPool)
    {
        if (GetCapsuleComponent())
        {
            // 죽을 때 NoCollision으로 바뀐 프로필을 완벽히 Pawn 기본값으로 복구합니다 (클라이언트 포함)
            GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
            GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
            GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel1, ECollisionResponse::ECR_Block);
        }
        if (GetMesh())
        {
            // 죽을 때 90도 꺾여 쓰러진(Dead) 시체 회전값을 기본값(서서 앞을 보는 -90도)으로 복구합니다
            GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
            GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
            GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
    }
    else
    {
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->StopMovementImmediately();
            GetCharacterMovement()->DisableMovement();
        }
        if (GetCapsuleComponent())
        {
            GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        if (GetMesh())
        {
            GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
}

void AFT_MinionCharacterBase::ReinitializeForPool(const FTransform& NewTransform, UFT_MinionData* NewData, const FGameplayTag& NewTeamTag)
{
    // [1] 완전 퓨어한 상태로 모든 어빌리티(액티브/패시브/뇌) 삭제
    if (HasAuthority() && AbilitySystemComponent)
    {
        AbilitySystemComponent->ClearAllAbilities();
    }

    // [2] 위치 초기화
    SetActorTransform(NewTransform, false, nullptr, ETeleportType::ResetPhysics);

    // [3] 컨트롤러 부활 보장
    if (HasAuthority() && GetController() == nullptr)
    {
        SpawnDefaultController();
    }

    // [4] 치매(과거 길찾기 흔적) 초기화
    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
        AIC->StopMovement();
        AIC->ClearFocus(EAIFocusPriority::Gameplay);
    }

    // [5] 데이터 교체
    SetMinionDataAndTeam(NewData, NewTeamTag);

    // [6] 상태이상 및 사망 태그 완전 소각
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetLooseGameplayTagCount(FTTags::FTStates::Core::Dead, 0);
        
        FGameplayEffectQuery Query;
        AbilitySystemComponent->RemoveActiveEffects(Query);
    }

    // [7] 체력 등 기본 속성 롤백
    if (AbilitySystemComponent && NewData)
    {
        AbilitySystemComponent->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(), NewData->DefaultMaxHealth);
    }

    // [8] 시체 회전 복구 및 물리엔진(콜리전) 100% 부활
    if (GetMesh())
    {
        GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    }
    
    bIsActiveInPool = true;
    OnRep_IsActiveInPool();

    // [9] 이동 컴포넌트 족쇄 풀기 (중력, 회전 복구)
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
        GetCharacterMovement()->GravityScale = 1.0f; 
        GetCharacterMovement()->bOrientRotationToMovement = true; 
        GetCharacterMovement()->Velocity = FVector::ZeroVector; 
        bUseControllerRotationYaw = false; 
        GetCharacterMovement()->UpdateComponentVelocity();
    }
    
    // [10] 마치 처음 태어난 것처럼 인프라(모든 스킬, 메시, 재질, 무기 부착) 재가동
    if (HasAuthority())
    {
        InitializeMinionInfrastructures();
    }
}