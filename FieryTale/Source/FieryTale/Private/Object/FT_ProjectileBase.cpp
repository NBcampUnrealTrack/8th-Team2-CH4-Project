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
    bReplicates = true;
    SetReplicateMovement(true);

    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    SetRootComponent(SphereComponent);
    SphereComponent->SetSphereRadius(16.0f);
    
    SphereComponent->SetCollisionProfileName(TEXT("Projectile"));

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = 1500.0f;
    ProjectileMovement->MaxSpeed = 1500.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;

    // 최대 수명 안전장치 적용: 사출 후 적을 맞추지 못해도 5초가 지나면 월드에서 자동 소멸하도록 타이머를 예약합니다.
    // 이 한 줄을 통해 허공이나 맵 밖으로 날아간 투사체들의 메모리 누수 버그를 파쇄합니다.
    InitialLifeSpan = 5.0f;
}

void AFT_ProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    
    if (HasAuthority())
    {
       SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFT_ProjectileBase::OnProjectileOverlap);
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
       if (DamageEffectSpecHandle.IsValid())
       {
          TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());
       }

       Destroy();
    }
}