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
    // 멀티플레이어 환경 대응: 서버에서 스폰된 투사체의 기동 궤적과 속도를 클라이언트 머신으로 정밀 복제 동기화합니다.
    bReplicates = true;
    SetReplicateMovement(true);

    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    SetRootComponent(SphereComponent);
    SphereComponent->SetSphereRadius(16.0f);
    
    // 순정 투사체 프리셋으로 복귀: 생명체(Pawn)와는 부드럽게 오버랩 통과하고, 지형/벽과는 블록 충돌을 일으키는 순정 프로필입니다.
    SphereComponent->SetCollisionProfileName(TEXT("Projectile"));

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1500.0f;
    ProjectileMovement->MaxSpeed = 1500.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;

    // 최대 수명 안전장치 적용: 공중에 가로막히거나 맵 밖으로 무한히 탈출하는 탄환들의 메모리 누수 버그를 파쇄하기 위해 5초 뒤 자동 소멸을 예약합니다.
    InitialLifeSpan = 5.0f;
}

void AFT_ProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 권한 필터링 완착: 클라이언트의 예측 오작동 및 부정 패킷 사출을 완벽 차단하기 위해, 오직 호스트/데디케이트 서버 권한 환경에서만 피격 오버랩 델리게이트를 개통합니다.
    if (HasAuthority())
    {
       SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFT_ProjectileBase::OnProjectileOverlap);
    }
}

void AFT_ProjectileBase::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 자가 피격 예외 처리: 충돌한 대상이 없거나 시전자 본인, 혹은 투사체를 사출한 인스티게이터일 경우 타격 장부 연산을 즉시 거부합니다.
    if (!OtherActor || OtherActor == GetOwner() || OtherActor == Cast<AActor>(GetInstigator()))
    {
       return;
    }

    AActor* MyInstigator = GetInstigator();
    if (!MyInstigator) return;

    UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyInstigator);
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

    // 피아식별 철저 검문소: 시전자진영과 피격자진영의 게임플레이 태그(Team_Blue / Team_Red)를 실시간 대조하여 아군일 경우 대미지 없이 그대로 관통시킵니다.
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

    // 적중 판정 및 즉시 파쇄: 타깃의 GAS 컴포넌트 실체가 확인되면, 어빌리티 단에서 위임받은 대미지 계산서 알맹이를 주입한 뒤 투사체 액터를 월드에서 청정 소멸시킵니다.
    if (TargetASC)
    {
       if (DamageEffectSpecHandle.IsValid())
       {
          TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
       }

       Destroy();
    }
}