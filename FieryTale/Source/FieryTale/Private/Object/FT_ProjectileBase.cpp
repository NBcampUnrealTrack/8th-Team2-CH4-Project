// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_ProjectileBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h" 
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTags/FTTags.h"

AFT_ProjectileBase::AFT_ProjectileBase()
{
    // 멀티플레이어 환경을 위한 액터 및 무브먼트 복제(Replication) 설정
    SetReplicates(true);
    SetReplicateMovement(true);

    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    SetRootComponent(SphereComponent);
    SphereComponent->SetSphereRadius(16.0f);
    
    // 캐릭터와는 오버랩, 지형과는 블록 히트 판정을 가지는 콜리전 프로필 설정
    SphereComponent->SetCollisionProfileName(TEXT("Projectile"));

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1500.0f;
    ProjectileMovement->MaxSpeed = 1500.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;

    // 최대 생존 수명 설정 (5초 후 자동 소멸)
    InitialLifeSpan = 5.0f;

    // 충돌 폭발 플래그 초기화
    bExploded = false;
}

void AFT_ProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 서버 권한 하에서만 오버랩 및 히트 델리게이트 바인딩 수행
    if (HasAuthority())
    {
        SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFT_ProjectileBase::OnProjectileOverlap);
        SphereComponent->OnComponentHit.AddDynamic(this, &AFT_ProjectileBase::OnProjectileHit);
    }
}

void AFT_ProjectileBase::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 중복 파괴 가드: 이미 액터 파괴 선로에 진입했거나 이미 폭발했다면 차단
    if (IsActorBeingDestroyed() || bExploded) return;

    // GetInstigator() 캐스팅 오버헤드 걷어내고 직접 비교로 바이패스
    if (!OtherActor || OtherActor == GetOwner() || OtherActor == GetInstigator())
    {
       return;
    }

    AActor* MyInstigator = GetInstigator();
    if (!MyInstigator) return;

    UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyInstigator);
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

    // 진영 태그 기반 피아식별 검문 (아군 오사 대미지 차단)
    if (InstigatorASC && TargetASC)
    {
        // 타깃이 이미 사망 상태라면 연산을 차단하고 즉시 소멸 처리
        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
        {
            ExplodeAndDestroy();
            return;
        }

        bool bIsSameTeam = false;

        if (InstigatorASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && 
            TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
        {
            bIsSameTeam = true;
        }
        else if (InstigatorASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && 
                 TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
        {
            bIsSameTeam = true;
        }

        // 관통 제어선 명확화: 아군일 경우 대미지 연산 및 하단 소멸 배관을 모두 스킵하고 관통 처리
        if (bIsSameTeam)
        {
            return;
        }
    }
    
    // [대미지 정산 및 투사체 소멸 파이프라인]
    if (TargetASC)
    {
        if (DamageEffectSpecHandle.IsValid() && DamageEffectSpecHandle.Data.IsValid())
        {
            // GAS 데이터 무결성 락인: 복사 생성자를 통해 독립 사본 장부 생성
            FGameplayEffectSpec LocalSpec(*DamageEffectSpecHandle.Data.Get());
            FGameplayEffectContextHandle Context = LocalSpec.GetContext().Duplicate();

            FHitResult FinalHitResult;
            if (bFromSweep)
            {
                FinalHitResult = SweepResult;
            }
            else
            {
                USceneComponent* RootComp = OtherActor->GetRootComponent();
                UPrimitiveComponent* TargetPrimitive = OtherComp ? OtherComp : (RootComp ? Cast<UPrimitiveComponent>(RootComp) : nullptr);
              
                // 💡 [오타 완치]: 내부 변수명을 FinalHitResult로 완벽하게 정순 통일 타설했습니다.
                FinalHitResult.Component = TargetPrimitive ? TargetPrimitive : SphereComponent;
                FinalHitResult.Location = GetActorLocation();
                FinalHitResult.ImpactPoint = GetActorLocation();
                FinalHitResult.HitObjectHandle = OtherActor; 
              
                if (ProjectileMovement)
                {
                    FinalHitResult.ImpactNormal = -ProjectileMovement->Velocity.GetSafeNormal();
                }
            }

            // 복사본 컨텍스트에 최종 적중 장부 주입 후 동기화
            Context.AddHitResult(FinalHitResult, true);
            LocalSpec.SetContext(Context);

            // 노멀 어택과 100% 동일하게 HitResult로부터 TargetDataHandle을 패킹합니다.
            FGameplayAbilityTargetDataHandle TargetDataHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(FinalHitResult);

            // TargetData의 순정 엘리먼트 직접 실행기를 구동하여 GEEC_Damage 연산소를 완벽하게 기폭합니다.
            if (TargetDataHandle.IsValid(0))
            {
                TargetDataHandle.Get(0)->ApplyGameplayEffectSpec(LocalSpec);
            }
            else
            {
                TargetASC->ApplyGameplayEffectSpecToSelf(LocalSpec);
            }
        }
    }
    // TargetASC가 없는 일반 오브젝트나 적대 대상에 닿았다면 예외 없이 투사체 소멸
    ExplodeAndDestroy();
}

void AFT_ProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (IsActorBeingDestroyed() || bExploded) return;

    // 지형, 벽면, 구조물 등 블록 콜리전에 충돌 시 즉각 파쇄 소각
    ExplodeAndDestroy();
}

void AFT_ProjectileBase::ExplodeAndDestroy()
{
    if (bExploded) return;

    if (HasAuthority())
    {
        bExploded = true;
        
        // 서버에서 액터가 명시적으로 파괴되었음을 네이티브 TearOff 네트워크 상태선으로 전파합니다.
        TearOff();
        
        if (!IsActorBeingDestroyed())
        {
            Destroy();
        }
    }
}

// 넷코드 동기화 라이프사이클 안착
void AFT_ProjectileBase::Destroyed()
{
    // 소멸 방식에 상관없이 컴포넌트 유효 가드 타설 후 안전 동결
    if (ProjectileMovement)
    {
        ProjectileMovement->StopMovementImmediately();
    }
    
    if (SphereComponent)
    {
        SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 수명 계산식 무결성 마감: 자연 만료선(0.01초 이하로 전소된 상태)이 아닐 때 파괴 패킷을 수신한 거라면
    // 서버측 플래그 복제가 지연되더라도 원격 클라이언트에서 100% 명중 폭발 연출을 격발하도록 보장 처리합니다.
    const bool bIsExplodedOnClient = bExploded || GetTearOff() || (GetLifeSpan() > 0.01f);
    if (bIsExplodedOnClient)
    {
        // 향후 피격 이펙트 및 사운드는 이곳에서 GameplayCue 태그를 격발하거나 Niagara를 로컬 스폰합니다.
    }

    Super::Destroyed();
}