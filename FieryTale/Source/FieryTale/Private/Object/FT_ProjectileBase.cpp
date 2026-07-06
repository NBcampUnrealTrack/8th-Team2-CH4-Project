// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_ProjectileBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"


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

    // [순정 GAS 타격 인프라 연동]
    // 충돌한 타겟 대상이 GAS 아키텍처를 준수하여 내부에 ASC 컴포넌트를 소유하고 있는지 장부를 견인합니다.
    if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
    {
       // 시전 스킬 단(앨리스, 빨간망토, 미니언 등)에서 지연 생성 체계를 통해 완벽하게 밀봉 토스해 준 대미지 계산서가 유효한지 검수
       if (DamageEffectSpecHandle.IsValid())
       {
          // 타겟 대상의 ASC 코어 통장에 최종 조립 완료된 화력 수치 및 효과 스펙을 다이렉트로 확정 기입 처리합니다.
          TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
       }

       // 대미지 배달 공정이 무사히 완결되었으므로 멀티플레이 리플리케이션 레이어 하에 서버 단에서 투사체 자산을 청정하게 소멸합니다.
       Destroy();
    }
}