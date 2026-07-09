// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_ProjectileBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTags/FTTags.h"

AFT_ProjectileBase::AFT_ProjectileBase()
{
    // 언리얼 5 순정 멀티플레이어 리플리케이션 가동 표준 명세를 확정합니다.
    SetReplicates(true);
    SetReplicateMovement(true);

    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    SetRootComponent(SphereComponent);
    SphereComponent->SetSphereRadius(16.0f);
    
    // 생명체와는 오버랩, 지형과는 블록 히트 판정을 내는 순정 프로필을 적용합니다.
    SphereComponent->SetCollisionProfileName(TEXT("Projectile"));

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1500.0f;
    ProjectileMovement->MaxSpeed = 1500.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;

    InitialLifeSpan = 5.0f;
}

void AFT_ProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 이중 충돌 센서 가드선 타설: 서버 주권 영역에서만 생명체 관측용 오버랩선과 지형 및 벽면 충돌용 히트 선로를 동시 격발 개통합니다.
    if (HasAuthority())
    {
        SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFT_ProjectileBase::OnProjectileOverlap);
        SphereComponent->OnComponentHit.AddDynamic(this, &AFT_ProjectileBase::OnProjectileHit);
    }
}

void AFT_ProjectileBase::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == GetOwner() || OtherActor == Cast<AActor>(GetInstigator()))
    {
       return;
    }

    AActor* MyInstigator = GetInstigator();
    if (!MyInstigator) return;

    UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyInstigator);
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

    // 피아식별 교차 검문선 가동: 아군 타격으로 인한 팀킬 대미지 연산을 원천 차단합니다.
    if (InstigatorASC && TargetASC)
    {
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

        if (bIsSameTeam)
        {
            return;
        }
    }

    if (TargetASC)
    {
       // 대미지 정산소 배달 확정
       if (DamageEffectSpecHandle.IsValid())
       {
          TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
       }

       // 고스트 유령 패킷 안락사: 파괴 선언 직후 물리 기동과 콜리전을 전면 동결 차단하여 클라이언트 화면에 탄환 좀비가 복제되어 날아가는 릭을 원천 분쇄합니다.
       ExplodeAndDestroy();
    }
}

// 지형 및 벽면 블록 히트 최종 보수 구역
void AFT_ProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 벽이나 포탑 구조물, 지형에 박혔다면 연산 낭비 없이 즉각 파쇄 소각합니다.
    ExplodeAndDestroy();
}

void AFT_ProjectileBase::ExplodeAndDestroy()
{
    // 원자적 강제 제동 파이프라인
    if (ProjectileMovement)
    {
        ProjectileMovement->StopMovementImmediately();
    }
    
    if (SphereComponent)
    {
        SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    // 향후 이 구역에 피격 이펙트 사출용 게임플레이 큐를 연동하면 완벽한 화면 연출 동기화가 이뤄집니다.

    Destroy();
}