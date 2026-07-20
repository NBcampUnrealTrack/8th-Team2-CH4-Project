#include "Level/FTTurret.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTags/FTTags.h"
#include "Engine/DataTable.h"
#include "Level/FTStructureData.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Character/FTCharacterBase.h"
#include "Object/FT_ProjectileBase.h"
#include "UI/Health/FTHealthWidgetComponent.h"
#include "Core/Game/FTGameMode.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

// 포탑 생성 및 초기화
AFTTurret::AFTTurret()
{
    bReplicates = true;

    TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
    SetRootComponent(TurretMesh);
    TurretMesh->SetCollisionProfileName(TEXT("BlockAllGeneric"));

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(TurretMesh);
    CollisionBox->SetBoxExtent(FVector(100.f, 100.f, 200.f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    CollisionBox->SetGenerateOverlapEvents(true);

    LaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserMesh"));
    LaserMesh->SetupAttachment(TurretMesh);
    LaserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LaserMesh->SetUsingAbsoluteLocation(true);
    LaserMesh->SetUsingAbsoluteRotation(true);
    LaserMesh->SetUsingAbsoluteScale(true);

    DebrisMesh = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DebrisMesh"));
    DebrisMesh->SetupAttachment(TurretMesh);

    TurretTeam = EFTTurretTeam::BlueTeam;
    TurretPosition = EFTTurretPosition::NonePosition;
    DetectionRange = 1200.0f;
    AttackCooldown = 1.5f;
    HomingAcceleration = 50000.0f;
    DestructionImpulseRadius = 500.0f;
    DestructionImpulseStrength = 10000.0f;
    CurrentTarget = nullptr;
    bCanAttack = true;
    TurretDummyInstigator = nullptr;
}

// 변수 네트워크 리플리케이션 설정
void AFTTurret::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AFTTurret, CurrentTarget);
    DOREPLIFETIME(AFTTurret, TurretTeam);
    DOREPLIFETIME(AFTTurret, TurretPosition);
}

// 포탑 게임 시작 로직 실행
void AFTTurret::BeginPlay()
{
    float TargetMaxHealth = 1000.0f;

    if (TurretDataTable && !DataRowName.IsNone())
    {
        if (FFTStructureData* RowData = TurretDataTable->FindRow<FFTStructureData>(DataRowName, TEXT("")))
        {
            TargetMaxHealth = RowData->MaxHealth;
        }
    }

    if (AttributeSet && AbilitySystemComponent)
    {
        AttributeSet->InitMaxHealth(TargetMaxHealth);
        AttributeSet->InitHealth(TargetMaxHealth);
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &AFTTowerBase::OnHealthChanged);
    }

    Super::BeginPlay();

    if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>())
    {
        if (UFTHealthWidgetComponent* FTHealthComp = Cast<UFTHealthWidgetComponent>(HealthWidgetComp))
        {
            FTHealthComp->UpdateASCBinding();
        }
    }

    if (LaserMesh)
    {
        LaserMesh->SetVisibility(false);
    }

    GetWorld()->GetTimerManager().SetTimer(TargetTrackingTimerHandle, this, &AFTTurret::TrackNearestEnemy, 0.03f, true);
}

// 팀 태그 반환
FGameplayTag AFTTurret::GetTeamTag() const
{
    return (TurretTeam == EFTTurretTeam::BlueTeam) ? FTTags::FTFaction::Team_Blue : FTTags::FTFaction::Team_Red;
}

// 구조물 태그 반환
FGameplayTag AFTTurret::GetStructureTag() const
{
    return FTTags::FTObjectType::Structure_Turret;
}

// 파괴 시각 및 물리 효과 처리
void AFTTurret::PerformDestructionEffects()
{
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(FTTags::FTCombat::Structure_Muted);
    }

    GetWorld()->GetTimerManager().ClearTimer(TargetTrackingTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);

    if (CollisionBox)
    {
        CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (TurretMesh)
    {
        TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (GetNetMode() != NM_DedicatedServer)
    {
        if (LaserMesh)
        {
            LaserMesh->SetVisibility(false);
        }

        if (UWidgetComponent* HealthWidgetComp = FindComponentByClass<UWidgetComponent>())
        {
            HealthWidgetComp->SetVisibility(false);
        }

        if (TurretMesh)
        {
            TurretMesh->SetVisibility(false);
        }

        if (DebrisMesh)
        {
            DebrisMesh->SetVisibility(true);
            DebrisMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
            DebrisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            DebrisMesh->SetCollisionProfileName(TEXT("Destructible"));
            DebrisMesh->SetSimulatePhysics(true);
            DebrisMesh->AddRadialImpulse(GetActorLocation(), DestructionImpulseRadius, DestructionImpulseStrength, ERadialImpulseFalloff::RIF_Linear, true);
        }

        if (DestructionSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, DestructionSound, GetActorLocation());
        }

        if (DestructionEffect)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DestructionEffect, GetActorLocation(), GetActorRotation());
        }
    }

    OnTurretDestroyed();
}

// 게임 모드에 파괴 알림
void AFTTurret::NotifyGameMode()
{
    if (UWorld* World = GetWorld())
    {
        if (AFTGameMode* GameMode = World->GetAuthGameMode<AFTGameMode>())
        {
            GameMode->TurretDestroyed(this);
        }
    }
    SetLifeSpan(5.0f);
    if (TurretDummyInstigator)
    {
        TurretDummyInstigator->SetLifeSpan(10.0f);
    }
}

// 범위 내 적 감지 및 타겟 설정
void AFTTurret::TrackNearestEnemy()
{
    if (bIsDying) return;

    if (HasAuthority())
    {
        if (CurrentTarget && (GetDistanceTo(CurrentTarget) > DetectionRange))
        {
            CurrentTarget = nullptr;
        }

        if (CurrentTarget)
        {
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CurrentTarget);
            if (TargetASC && TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
            {
                CurrentTarget = nullptr;
            }
        }

        if (!CurrentTarget)
        {
            TArray<AActor*> FoundActors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFTCharacterBase::StaticClass(), FoundActors);

            AActor* NearestEnemy = nullptr;
            float ClosestDistance = DetectionRange;

            for (AActor* Actor : FoundActors)
            {
                AFTCharacterBase* EnemyChar = Cast<AFTCharacterBase>(Actor);
                if (!EnemyChar) continue;

                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EnemyChar);
                if (!TargetASC || TargetASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead)) continue;

                bool bIsEnemy = false;
                if (TurretTeam == EFTTurretTeam::BlueTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsEnemy = true;
                if (TurretTeam == EFTTurretTeam::RedTeam && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsEnemy = true;

                if (bIsEnemy)
                {
                    float Distance = GetDistanceTo(EnemyChar);
                    if (Distance < ClosestDistance)
                    {
                        ClosestDistance = Distance;
                        NearestEnemy = EnemyChar;
                    }
                }
            }
            CurrentTarget = NearestEnemy;
        }

        if (CurrentTarget && bCanAttack)
        {
            FireProjectile();
        }
    }

    if (CurrentTarget && LaserMesh)
    {
        LaserMesh->SetVisibility(true);
        FVector StartLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f);
        FVector EndLocation = CurrentTarget->GetActorLocation();
        FVector MidLocation = (StartLocation + EndLocation) * 0.5f;
        float Distance = FVector::Distance(StartLocation, EndLocation);
        FRotator LaserRot = UKismetMathLibrary::FindLookAtRotation(StartLocation, EndLocation);

        LaserMesh->SetWorldLocation(MidLocation);
        LaserMesh->SetWorldRotation(LaserRot);
        LaserMesh->SetWorldScale3D(FVector(Distance / 100.0f, 0.05f, 0.05f));
    }
    else if (LaserMesh)
    {
        LaserMesh->SetVisibility(false);
    }
}

// 투사체 발사 처리
void AFTTurret::FireProjectile()
{
    if (!HasAuthority() || !CurrentTarget || !ProjectileClass) return;

    bCanAttack = false;

    if (!TurretDummyInstigator)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = this;
        TurretDummyInstigator = GetWorld()->SpawnActor<APawn>(APawn::StaticClass(), GetActorLocation(), GetActorRotation(), SpawnParams);

        if (TurretDummyInstigator)
        {
            TurretDummyInstigator->SetReplicates(false);
            TurretDummyInstigator->SetActorHiddenInGame(true);
            TurretDummyInstigator->SetActorEnableCollision(false);
            TurretDummyInstigator->SetCanBeDamaged(false);

            UFT_AbilitySystemComponent* DummyASC = NewObject<UFT_AbilitySystemComponent>(TurretDummyInstigator, TEXT("DummyASC"));
            if (DummyASC)
            {
                DummyASC->RegisterComponent();
                DummyASC->AddLooseGameplayTag(GetTeamTag());
            }
        }
    }

    FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 200.0f);
    FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, CurrentTarget->GetActorLocation());
    FTransform SpawnTransform(SpawnRotation, SpawnLocation);

    AFT_ProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AFT_ProjectileBase>(ProjectileClass, SpawnTransform, this, TurretDummyInstigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (Projectile)
    {
        if (UProjectileMovementComponent* MovementComp = Projectile->FindComponentByClass<UProjectileMovementComponent>())
        {
            MovementComp->bIsHomingProjectile = true;
            MovementComp->HomingAccelerationMagnitude = HomingAcceleration;
            if (CurrentTarget && CurrentTarget->GetRootComponent())
            {
                MovementComp->HomingTargetComponent = CurrentTarget->GetRootComponent();
            }
        }

        if (AttackDamageEffectClass && AbilitySystemComponent)
        {
            FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
            EffectContext.AddInstigator(TurretDummyInstigator, this);
            FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(AttackDamageEffectClass, 1.0f, EffectContext);
            Projectile->DamageEffectSpecHandle = SpecHandle;
        }

        UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);

        FTimerHandle RpcDelayHandle;
        GetWorld()->GetTimerManager().SetTimer(RpcDelayHandle, FTimerDelegate::CreateLambda([this, Projectile, CurrentTarget = this->CurrentTarget]() {
            if (IsValid(this) && IsValid(Projectile) && IsValid(CurrentTarget))
            {
                Multicast_SyncProjectileHoming(Projectile, CurrentTarget);
            }
        }), 0.1f, false);
    }

    GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, FTimerDelegate::CreateLambda([this]() {
        bCanAttack = true;
    }), AttackCooldown, false);
}

// 투사체 호밍 동기화 멀티캐스트
void AFTTurret::Multicast_SyncProjectileHoming_Implementation(AFT_ProjectileBase* Projectile, AActor* Target)
{
    if (Projectile && Target)
    {
        if (UProjectileMovementComponent* MovementComp = Projectile->FindComponentByClass<UProjectileMovementComponent>())
        {
            MovementComp->bIsHomingProjectile = true;
            MovementComp->HomingAccelerationMagnitude = HomingAcceleration;
            MovementComp->HomingTargetComponent = Target->GetRootComponent();
        }
    }
}