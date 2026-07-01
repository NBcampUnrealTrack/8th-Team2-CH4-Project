// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/UltimateSkill/FT_WolfRoarAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayTags/FTTags.h"

UFT_WolfRoarAbility::UFT_WolfRoarAbility()
{
    // [인스턴싱 정책] 캐릭터마다 고유의 궁극기 스펙과 런타임 타깃 장부를 격리 관리하기 위해 액터당 인스턴스화 모드로 관리합니다.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [네트워크 실행 정책] 키 입력 즉시 로컬 화면에서 하울링 시각 효과가 켜지도록 클라이언트 선입력 예측 시스템을 구동합니다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [에셋 태그 주입] 궁극기 범주 분류 및 연계 시스템 식별을 위해 전용 에셋 태그를 완착합니다.
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FTTags::FTAbilities::UltimateSkill); 
    SetAssetTags(AssetTags);

    // [기획 사양 기본값 안전 세팅 구역]
    HuntRadius = 800.0f;       // 부채꼴 포효가 뻗어나갈 최대 사거리 8미터 명세
    ConeAngle = 90.0f;         // 전방 조준선 기준 좌우 각각 45도씩 펼쳐지는 총 90도 부채꼴 각도 명세
    BaseDamageValue = 150.0f;  // 공포의 포효에 적중당한 대상에게 가동할 기저 피해량 수치 명세
}

void UFT_WolfRoarAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // [GAS 마스터 규격] 마스터 베이스 클래스의 게이지 비용 소모 파이프라인을 가동하여 비용 소모 및 검증을 최상단 관문에서 처리합니다.
    // 내부적으로 코스트 차감과 쿨다운 적용을 원자적으로 일괄 수행합니다.
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* OwnerActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!OwnerActor || !SourceASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (bDrawDebugs)
    {
        UE_LOG(LogTemp, Log, TEXT("WolfRoar Activated: 할머니를 삼켰던 이빨로 적들을물어뜯어라."));
    }

    // =========================================================================
    // [M-1 및 C2660 버그 완치 가드벽]
    // 다중 타깃 오버랩 스캔 및 타인의 ASC 장부에 원격으로 대미지, 속박 GE를 주입하는 민감한 연산은
    // 주권 권한을 지닌 데디케이트 서버(K2_HasAuthority) 내부에서만 가동되도록 완벽히 가둡니다.
    // 이로 인해 클라이언트 머신의 불법적인 타인 스탯 오예측 및 롤백 현상이 원천 차단됩니다.
    // =========================================================================
    if (K2_HasAuthority())
    {
        // --- 전방 부채꼴 볼륨 확정 타격 물리 스캔 구역 ---
        FVector StartLocation = OwnerActor->GetActorLocation();
        FVector ForwardVector = OwnerActor->GetActorForwardVector();
        
        // [Z축 왜곡 선제 차단] 3차원 고저차 경사로나 지형 각도로 인해 정면 조준선 벡터가 평면 왜곡되는 현상을 지웁니다.
        ForwardVector.Z = 0.f; 
        ForwardVector.Normalize();

        TArray<FOverlapResult> OverlapResults;
        // 1차 필터링: 부채꼴 리치 반경에 맞추어 사거리 내 원형 구체 형태의 1차 충돌 풀을 휩쓸기 스캔합니다.
        FCollisionShape ScanSphere = FCollisionShape::MakeSphere(HuntRadius);
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(OwnerActor);

        bool bHasOverlap = OwnerActor->GetWorld()->OverlapMultiByChannel(
            OverlapResults,
            StartLocation,
            FQuat::Identity,
            ECollisionChannel::ECC_Pawn, 
            ScanSphere,
            QueryParams
        );

        if (bHasOverlap)
        {
            // [삼각함수 내적 기준선 수립] 기획 지정 부채꼴 각도의 절반값을 코사인 라디안 수치로 변환하여 유효 임계 점수를 확보합니다.
            float DotThreshold = FMath::Cos(FMath::DegreesToRadians(ConeAngle * 0.5f));

            for (const FOverlapResult& Result : OverlapResults)
            {
                AActor* TargetActor = Result.GetActor();
                if (!TargetActor) continue;

                // [지형 높낮이 판정 가드] 2층 지형이나 공중 점프 상태의 적도 수평 평면상에서 공평하게 판정받도록 변위 벡터의 Z축 편차를 무력화합니다.
                FVector TargetDirection = (TargetActor->GetActorLocation() - StartLocation);
                TargetDirection.Z = 0.f; 
                TargetDirection.Normalize();

                // 시전자 전방 시선 방향과 적 영웅 위치 방향 간의 코사인 내적 연산을 단행합니다.
                float CurrentDot = FVector::DotProduct(ForwardVector, TargetDirection);

                // [부채꼴 범위 필터링] 연산된 내적 임계 점수보다 낮다면 시야각 외곽에 존재하는 적이므로 판정에서 제외합니다.
                if (CurrentDot < DotThreshold) continue;

                UAbilitySystemComponent* TargetASC = TargetActor->GetComponentByClass<UAbilitySystemComponent>();

                if (TargetASC)
                {
                    // [AOS 피아식별 이중 안전망] 아군 진영 태그를 실시간 대조하여 난전 중 아군 오사 피해 및 오인 디버프 주입 릭을 원천 방쇄합니다.
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) continue;
                    if (SourceASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && TargetASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) continue;

                    // [대미지 이펙트 주입 파이프라인]
                    // 런타임 흐름: BaseDamageValue 수치를 피해 연산 우체통에 패스하여 마스터 속성 집합의 대미지 슬롯으로 발송합니다.
                    if (DamageGameplayEffectClass)
                    {
                        FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffectClass, GetAbilityLevel());
                        if (DamageSpecHandle.IsValid())
                        {
                            // C-1 버그 완치 연동: 대미지 수신 태그 장부 상수를 밀어 넣어 GEEC_Damage 수선본과 완벽히 동기화합니다.
                            DamageSpecHandle.Data->SetSetByCallerMagnitude(FTTags::FTCombat::Damage, BaseDamageValue);
                            TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
                        }
                    }

                    // [군중 제어 속박 이펙트 주입 파이프라인]
                    // 런타임 흐름: 조건이 일치하는 부채꼴 범위 내 모든 적들의 발을 묶는 디버프 상태 태그를 대상 장부에 직접 낙인합니다.
                    if (RootGameplayEffectClass)
                    {
                        FGameplayEffectSpecHandle RootSpecHandle = MakeOutgoingGameplayEffectSpec(RootGameplayEffectClass, GetAbilityLevel());
                        if (RootSpecHandle.IsValid())
                        {
                            TargetASC->ApplyGameplayEffectSpecToSelf(*RootSpecHandle.Data.Get());
                        }
                    }
                }
            }
        }
    }

    // 포효 격발 및 디버프 주입 시퀀스가 완료되면 수명 주기를 클린하게 격리 종료합니다.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}