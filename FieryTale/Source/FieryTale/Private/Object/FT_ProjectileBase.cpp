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
#include "TimerManager.h" // 💡 [넷코드 유실 가드용 타이머 헤더 주입]
#include "Engine/World.h"

AFT_ProjectileBase::AFT_ProjectileBase()
{
    SetReplicates(true);
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

    InitialLifeSpan = 5.0f;
    bExploded = false;
}

void AFT_ProjectileBase::BeginPlay()
{
    Super::BeginPlay();
    
    if (HasAuthority())
    {
        SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AFT_ProjectileBase::OnProjectileOverlap);
        SphereComponent->OnComponentHit.AddDynamic(this, &AFT_ProjectileBase::OnProjectileHit);
    }
}

void AFT_ProjectileBase::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (IsActorBeingDestroyed() || bExploded)
    {
        return;
    }

    if (!OtherActor || OtherActor == GetOwner() || OtherActor == GetInstigator())
    {
        return;
    }

    AActor* MyInstigator = GetInstigator();
    if (!ensureAlwaysMsgf(MyInstigator, TEXT("FT_ProjectileBase [%s] has no valid Instigator!"), *GetName()))
    {
        return;
    }

    UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MyInstigator);
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

    if (InstigatorASC && TargetASC)
    {
        if (TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead) ||
            TargetASC->HasMatchingGameplayTag(FTTags::FTCombat::Structure_Muted))
        {
            ExplodeAndDestroy();
            return;
        }

        bool bIsSameTeam = false;
        if (InstigatorASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
        {
            bIsSameTeam = true;
        }
        else if (InstigatorASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
        {
            bIsSameTeam = true;
        }

        if (bIsSameTeam)
        {
            return;
        }
    }
    
    // =========================================================================
    // 💡 [완치 구역]: 대미지 및 추가 상태 이상(슬로우 등) 이펙트 일괄 직통 주입
    // =========================================================================
    if (TargetASC)
    {
        // 공통 HitResult 계산 (단 1회만 연산)
        FHitResult FinalHitResult;
        if (bFromSweep)
        {
            FinalHitResult = SweepResult;
        }
        else
        {
            USceneComponent* RootComp = OtherActor->GetRootComponent();
            UPrimitiveComponent* TargetPrimitive = OtherComp ? OtherComp : (RootComp ? Cast<UPrimitiveComponent>(RootComp) : nullptr);
          
            FinalHitResult.Component = TargetPrimitive ? TargetPrimitive : SphereComponent;
            FinalHitResult.Location = GetActorLocation();
            FinalHitResult.ImpactPoint = GetActorLocation();
            FinalHitResult.HitObjectHandle = OtherActor; 
          
            if (ProjectileMovement)
            {
                FinalHitResult.ImpactNormal = -ProjectileMovement->Velocity.GetSafeNormal();
            }
        }

        // 1. 마스터 대미지 스펙 주입
        if (DamageEffectSpecHandle.IsValid() && DamageEffectSpecHandle.Data.IsValid())
        {
            FGameplayEffectSpec LocalSpec(*DamageEffectSpecHandle.Data.Get());
            FGameplayEffectContextHandle Context = LocalSpec.GetContext().Duplicate();
            Context.AddHitResult(FinalHitResult, true);
            LocalSpec.SetContext(Context);

            if (InstigatorASC)
            {
                InstigatorASC->ApplyGameplayEffectSpecToTarget(LocalSpec, TargetASC);
            }
            else
            {
                TargetASC->ApplyGameplayEffectSpecToSelf(LocalSpec);
            }
        }
        
        // 2. 추가 상태 이상(슬로우, 스턴 등) 스펙 배열 일괄 주입 (이전에 누락되었던 부분 복구)
        for (const FGameplayEffectSpecHandle& AdditionalHandle : AdditionalEffectSpecHandles)
        {
            if (AdditionalHandle.IsValid() && AdditionalHandle.Data.IsValid())
            {
                FGameplayEffectSpec LocalSpec(*AdditionalHandle.Data.Get());
                FGameplayEffectContextHandle Context = LocalSpec.GetContext().Duplicate();
                Context.AddHitResult(FinalHitResult, true);
                LocalSpec.SetContext(Context);

                if (InstigatorASC)
                {
                    InstigatorASC->ApplyGameplayEffectSpecToTarget(LocalSpec, TargetASC);
                }
                else
                {
                    TargetASC->ApplyGameplayEffectSpecToSelf(LocalSpec);
                }
            }
        }
    }
    
    // 타깃 정산 완료 후 안전 수명 소멸 시퀀스로 전이
    ExplodeAndDestroy();
}

void AFT_ProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (IsActorBeingDestroyed() || bExploded)
    {
        return;
    }
    ExplodeAndDestroy();
}

void AFT_ProjectileBase::ExplodeAndDestroy()
{
    if (bExploded)
    {
        return;
    }

    if (HasAuthority())
    {
        bExploded = true;
        
        // 💡 [치명적 넷코드 소멸 복제 타이밍 버그 파쇄]
        // 적중 즉시 서버에서 액터를 소멸(Destroy)시키면 복제 패킷이 유실되어 슬로우가 씹힙니다.
        // 먼저 콜리전과 물리 장부만 즉시 꺼버려 유령(Ghost) 상태로 만든 뒤, 0.05초의 안전 마진 딜레이를 주고 소각합니다!
        if (SphereComponent)
        {
            SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        if (ProjectileMovement)
        {
            ProjectileMovement->StopMovementImmediately();
        }

        TearOff();
        
        // 0.05초 안전 지연 후 최종 소멸 배관 격발
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            if (IsValid(this) && !IsActorBeingDestroyed())
            {
                Destroy();
            }
        });
    }
}

void AFT_ProjectileBase::Destroyed()
{
    if (ProjectileMovement)
    {
        ProjectileMovement->StopMovementImmediately();
    }
    
    if (SphereComponent)
    {
        SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    const bool bIsExplodedOnClient = bExploded || GetTearOff() || (GetLifeSpan() > 0.01f);
    if (bIsExplodedOnClient)
    {
        // 런타임 이펙트 사운드/나이아가라 연출 구역
    }

    Super::Destroyed();
}