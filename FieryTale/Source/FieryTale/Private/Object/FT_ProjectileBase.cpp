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
    // 멀티플레이어 환경에서의 정밀한 위치 동기화를 위한 네트워크 복제 활성화
    bReplicates = true;
    SetReplicateMovement(true);

    // 1. 구체 충돌체 인프라 완착
    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    SetRootComponent(SphereComponent);
    SphereComponent->SetSphereRadius(16.0f);
    
    // AOS 환경에 맞는 순정 충돌 프리셋 설정 (기본적으로 오버랩 전용으로 설계하여 물리 밀림 현상 차단)
    SphereComponent->SetCollisionProfileName(TEXT("Projectile"));

    // 2. 비주얼 메시 컴포넌트 연동
    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메시는 충돌 연산에서 제외하여 릭 소거

    // 3. 투사체 무브먼트 컴포넌트 개통 (기본 속도 설정 및 중력 무시)
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1500.0f;
    ProjectileMovement->MaxSpeed = 1500.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f; // AOS 평타 스킬 투사체는 무중력 직선 비행이 정석
}

void AFT_ProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    
    // [네트워크 보안 및 서버 주권 룰 완착]
    // 클라이언트 단의 변조 오버랩에 의한 불법 대미지 핵 조작을 원천 봉쇄하기 위해
    // 오직 밸리데이션 권한을 가진 서버(HasAuthority) 환경에서만 충돌 이벤트가 격발되도록 제한 바인딩합니다.
    if (HasAuthority())
    {
       SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFT_ProjectileBase::OnProjectileOverlap);
    }
}

void AFT_ProjectileBase::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 예외 필터링: 피격 대상이 없거나, 시전자 본인이거나, 나를 사출한 무기 주체인 경우 충돌 장부에서 무결하게 제외
    if (!OtherActor || OtherActor == GetOwner() || OtherActor == Cast<AActor>(GetInstigator()))
    {
       return;
    }

    // 1. 시전자(나를 쏜 영웅 혹은 미니언)의 코어 GAS 장부 견인
    AActor* MyInstigator = GetInstigator();
    if (!MyInstigator) return;

    UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyInstigator);
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

    // =========================================================================
    // [AOS 정석: 투사체 실시간 동적 피아식별 배관망]
    // 시전자와 피격 타깃 양측의 진영 태그(Blue/Red)를 런타임 역산 비교하여 아군 오폭을 원천 차단합니다.
    // =========================================================================
    if (InstigatorASC && TargetASC)
    {
        // 시전자가 블루팀인데 타깃도 블루팀이거나, 시전자가 레드팀인데 타깃도 레드팀인 경우 (아군 판정)
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

        // 아군으로 판명되면 투사체가 뚫고 지나가도록 대미지 처리 및 소멸을 생략하고 즉시 리턴(Pass)시킵니다.
        // 이를 통해 블루팀 아군 미니언이나 영웅이 앞을 가로막아도 투사체가 고스트처럼 관통하여 적에게 날아갑니다.
        if (bIsSameTeam)
        {
            return;
        }
    }

    // 2. 피아식별 검문소를 통과한 진짜 적군개체에게만 GAS 대미지 계산서 최종 확정 기입
    if (TargetASC)
    {
       // 시전 스킬 단에서 지연 생성 체계를 통해 완벽하게 밀봉 토스해 준 대미지 계산서가 유효한지 검수
       if (DamageEffectSpecHandle.IsValid())
       {
          // 타겟 대상의 ASC 코어 장부에 최종 조립 완료된 화력 수치 및 효과 스펙을 다이렉트로 확정 기입 처리합니다.
          TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
       }

       // 적을 명중시켜 대미지 배달 공정이 완결되었으므로 서버 단에서 투사체 자산을 청정하게 소멸합니다.
       Destroy();
    }
}