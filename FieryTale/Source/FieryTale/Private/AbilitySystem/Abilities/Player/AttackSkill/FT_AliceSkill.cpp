// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/AttackSkill/FT_AliceSkill.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h" 
#include "DrawDebugHelpers.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTags/FTTags.h"
#include "Object/FT_ProjectileBase.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Engine/OverlapResult.h"

UFT_AliceSkill::UFT_AliceSkill()
    : BaseDamage(30.0f) // 기획 명세: 시계 토끼 사출 스킬 기본 피해량
{
    // 인스턴싱 정책: 성능 최적화 및 실시간 데이터 격리를 위해 액터당 하나씩 독립 인스턴스화
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // 네트워크 실행 정책: 클라이언트 선예측 후 서버 검증(Local Predicted)으로 한타 시 즉각적인 입력 반응성 보장
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // GAS 에셋 태그 등록: 스킬 분류 장부에 공격 스킬 태그 주입
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::AttackSkill);
    SetAssetTags(AssetTags);
    
    // 쿨다운 태그 연동: 마우스 우클릭 스킬 공용 쿨다운 파이프라인(9초)과 자석 바인딩
    CooldownTag = FTTags::FTStates::Cooldown::RightClick;
}

void UFT_AliceSkill::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // [GAS 순정 시전 관문]: 마나 비용 및 재사용 대기시간(Cooldown)이 충족되었는지 검증 및 차감
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    // 1단계: 동적 해킹 레이더 기동
    // 앨리스의 참전 진영(블루/레드)을 실시간 역산하여 사거리 내의 적들만 정밀 스캔 마킹
    ScanHackingTargets();

    // 2단계: 비동기 몽타주 재생 및 CC기 인터럽트 가드 구축
    // 스킬 시전 애니메이션을 구동하고, 중간에 하드 CC를 맞으면 능력을 즉시 안전 파쇄
    bool bHasVisualTask = false;
    if (AttackMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, TEXT("AliceSkillTask"), AttackMontage, 1.0f);

        if (MontageTask)
        {
            // 애니메이션 정상 완료, 외부 방해(인터럽트), 강제 취소 델리게이트를 마감 콜백에 일괄 바인딩
            MontageTask->OnCompleted.AddDynamic(this, &UFT_AliceSkill::OnSkillMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UFT_AliceSkill::OnSkillMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UFT_AliceSkill::OnSkillMontageFinished);
            
            MontageTask->ReadyForActivation();
            bHasVisualTask = true;
        }
    }
    
    // 3단계: 실물 투사체 격발
    // 전방으로 대미지 계산서가 밀봉된 시계 토끼 오브젝트를 물리 사출
    FireClockRabbit();
    
    // 만약 애니메이션 자산이 등록 안 되어 비동기 태스크가 작동하지 않았다면 런타임 락을 방지하기 위해 즉시 능력 클리어
    if (!bHasVisualTask)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UFT_AliceSkill::ScanHackingTargets()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    UWorld* World = GetWorld();
    if (!Character || !World) return;
    
    // 시전자(앨리스) 본인의 코어 ASC 장부 견인
    UAbilitySystemComponent* OwnerASC = CurrentActorInfo->AbilitySystemComponent.Get();
    if (!OwnerASC) return;

    // [AOS 핵심: 실시간 피아식별 타겟 필터 역산]
    // 하드코딩을 타파하고 시전자의 팀 태그의 정반대 세력을 적으로 규정하는 동적 스위칭 알고리즘
    FGameplayTag TargetEnemyTag = FGameplayTag::EmptyTag;

    if (OwnerASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue))
    {
        // 내가 블루팀 영웅이다 : 타겟팅 레이더는 레드팀(Team_Red)만 솎아냄
        TargetEnemyTag = FTTags::FTFaction::Team_Red;
    }
    else if (OwnerASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red))
    {
        // 내가 레드팀 영웅이다 : 타겟팅 레이더는 블루팀(Team_Blue)만 솎아냄
        TargetEnemyTag = FTTags::FTFaction::Team_Blue;
    }
    
    // 관전 모드 및 무소속 상태에서의 비정상 오폭 버그 차단
    if (!TargetEnemyTag.IsValid()) return;
    
    // 트래킹 타겟 누적 왜곡 영구 방쇄를 위해 실시간 스캔 장부 초기화
    TrackedTargets.Empty();
    
    // 기획 스펙 명세: 해킹 스캔 레이더 반경 (800cm = 8.0미터 대형 구체)
    const float ScanRadius = 800.0f;
    FVector ScanCenter = Character->GetActorLocation();

    // 쿼리 매개변수 설정: 시전자 본인의 히트박스는 탐지 레이더선에서 제외(자가 해킹 방지)
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);
    
    // 멀티 겹침 채널 스캔 가동: ECC_Pawn 채널을 쓰는 모든 영웅 및 미니언 군단을 구체 범위로 레이더 포착
    TArray<FOverlapResult> OverlapResults;
    bool bHasOverlap = World->OverlapMultiByChannel(
        OverlapResults,
        ScanCenter,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(ScanRadius),
        QueryParams
    );

    if (bHasOverlap)
    {
        for (const FOverlapResult& Result : OverlapResults)
        {
            AActor* OverlappedActor = Result.GetActor();
            if (!OverlappedActor) continue;

            // [순정 GAS 피아식별 연동망]: 포착된 액터가 GAS 무결성 규격(Interface)을 준수하는지 검증
            if (IAbilitySystemInterface* GASInterface = Cast<IAbilitySystemInterface>(OverlappedActor))
            {
                if (UAbilitySystemComponent* TargetASC = GASInterface->GetAbilitySystemComponent())
                {
                    // 위에서 동적으로 계산해 낸 적군 진영 태그와 매칭되는 진짜 적들만 정밀 필터링
                    if (TargetASC->HasMatchingGameplayTag(TargetEnemyTag))
                    {
                        // 런타임 스캔 보관소 장부에 적 포인터 포착 등록
                        TrackedTargets.Add(OverlappedActor);
                        
                        // [순정 GAS 실시간 디버프 연동]: 적의 ASC 장부에 실시간 HackedMark(해킹 표식) 루즈 태그 낙인 각인
                        // (블루프린트/아트 단에서 이 태그를 감지하여 홀로그램 머리 위 UI 및 틴트 염색 연동 가능)
                        TargetASC->AddLooseGameplayTag(FTTags::FTStates::Buff::HackedMark);
                    }
                }
            }
        }
    }
    
    // 디버그 드로잉: 에디터 개발용 레이더 가시화 버퍼 구체 생성 (시핑 빌드 컴파일 시 소스 레벨에서 전량 자동 자동 증발 최적화)
#if !UE_BUILD_SHIPPING
    DrawDebugSphere(World, ScanCenter, ScanRadius, 16, FColor::Cyan, false, 1.0f, 0, 1.5f);
#endif
}

void UFT_AliceSkill::FireClockRabbit()
{
    AFTPlayerCharacterBase* Character = CurrentActorInfo ? Cast<AFTPlayerCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    if (!Character || !Character->GetWeaponData())
    {
        return;
    }

    // 데이터 에셋으로부터 투사체 무기 원본 스펙 로드
    UFT_WeaponData* WeaponData = Character->GetWeaponData();
    UWorld* World = GetWorld();

    // 종합 대미지 계산서 주입 및 사출 인프라 가동
    if (World && WeaponData->ProjectileClass && RabbitImpactEffectClass)
    {
        // 사출 트랜스폼 산출 (캐릭터 전방 높이 60cm 허리 라인 기준 정방향 락온)
        FVector SpawnLocation = Character->GetActorLocation() + FVector(0, 0, 60);
        FVector LaunchDirection = Character->GetActorForwardVector();
        FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = Character;
        SpawnParams.Instigator = Character;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // [GAS 순정 화력 계산서 발행]: 적중 시 폭발할 GameplayEffect의 스펙 명세서 패킷을 생성
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RabbitImpactEffectClass, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            // SetByCaller 변조 우체통망을 통해 기획 고유 대미지 수치(30.0f)를 명시적으로 계산서에 밀봉 주입
            SpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamage);
        }

        // 지연 생성(Deferred Spawn) 가동: 투사체가 월드에 실물로 태어나기 전 대기 상태 유도
        AFT_ProjectileBase* Projectile = World->SpawnActorDeferred<AFT_ProjectileBase>(
            WeaponData->ProjectileClass, SpawnTransform, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        
        if (Projectile)
        {
            // 완공된 대미지 명세서 장부를 투사체 내부 배관에 안전하게 토스 인도
            Projectile->DamageEffectSpecHandle = SpecHandle;
            
            // 데이터 인도가 완료되었으므로 월드에 최종 실물화 사출 가동
            Projectile->FinishSpawning(SpawnTransform);
        }
    }
}

void UFT_AliceSkill::OnSkillMontageFinished()
{
    // 스킬 모션 연출 혹은 CC기 파쇄가 완료된 순간, 즉시 EndAbility 관문으로 이관하여 장부 정리 가동
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFT_AliceSkill::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // [GAS 자원 수거 청정화 마감 공정]
    // 스킬 수명 주기가 종료되는 순간, 포착되었던 모든 적들의 머리 위 해킹 표식(HackedMark) 태그를 전량 깨끗하게 회수
    for (TObjectPtr<AActor> TargetActor : TrackedTargets)
    {
        // 템플릿 심볼 왜곡 방지를 위해 언리얼 순정 글로벌 ::IsValid 오브젝트 검증기 가동
        if (::IsValid(TargetActor))
        {
            if (IAbilitySystemInterface* GASInterface = Cast<IAbilitySystemInterface>(TargetActor))
            {
                if (UAbilitySystemComponent* TargetASC = GASInterface->GetAbilitySystemComponent())
                {
                    // 적 장부에서 해킹 낙인을 지워주어 비주얼 연출 및 디버프 상태를 클린 청소
                    TargetASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::HackedMark);
                }
            }
        }
    }

    // 마스터 상위 클래스로 수명 주기 이관 및 스킬 최종 락 해제
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}