// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Minion/FT_Minion_Brain.h"
#include "AbilitySystemComponent.h"
#include "Character/FTCharacterBase.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameplayTags/FTTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AbilitySystemInterface.h" 
#include "Character/Minion/FT_MinionCharacterBase.h"
#include "Object/FT_WayPoint.h"

UFT_Minion_Brain::UFT_Minion_Brain()
{
    // 인스턴싱 정책: 미니언 개체별로 뇌세포 틱(타이머)을 독립 구동하기 위해 액터당 인스턴스 격리 생성
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    
    // [GAS 마스터 CC 가드선 완착]
    // 기절(Stunned) 디버프 상태 레이어가 가동 중일 때는 AI 사고 회로 자체를 셧다운 봉쇄합니다.
    ActivationBlockedTags.AddTag(FTTags::FTStates::Debuff::Stunned);
}

void UFT_Minion_Brain::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    if (GetWorld())
    {
        // 기획 명세: 지정된 스캔인터벌 주기에 맞춰 미니언의 핵심 AI 전술 루프를 상시 무한 반복 구동합니다.
        GetWorld()->GetTimerManager().SetTimer(
            AIExecuteTimerHandle,
            this,
            &UFT_Minion_Brain::ExecuteAILogic,
            ScanInterval,
            true
        );
    }
}

void UFT_Minion_Brain::ExecuteAILogic()
{
    AFTCharacterBase* AvatarChar = Cast<AFTCharacterBase>(GetAvatarActorFromActorInfo());
    if (!AvatarChar) return;

    AAIController* AIC = Cast<AAIController>(AvatarChar->GetController());
    if (!AIC) return;

    UAbilitySystemComponent* MyASC = AvatarChar->GetAbilitySystemComponent();
    if (!MyASC || MyASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
    {
        // 미니언 사망 시 즉시 AI 작동 타이머를 소각 해제하고 뇌세포 어빌리티 종결
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    UBlackboardComponent* BB = AIC->GetBlackboardComponent();
    if (!BB) return;

    // 배관 융합 코어: 컨트롤러의 AIPerception이 인양하여 블랙보드 장부에 정비해 둔 진짜 타깃을 직접 인양합니다.
    AActor* BestTarget = Cast<AActor>(BB->GetValueAsObject(TEXT("TargetEnemy")));

    if (BestTarget)
    {
        // 시야 내에 타깃 포착 시 무빙 포커스를 적으로 고정 (정면 상시 락온 상태 유도)
        AIC->SetFocus(BestTarget);
        float DistanceToTarget = FVector::Dist(AvatarChar->GetActorLocation(), BestTarget->GetActorLocation());

        if (DistanceToTarget <= AttackAcceptanceRadius)
        {
            // [교전 상태]: 사거리 내부 도달 시 정지 후 평타 GA 격발 유도
            AIC->StopMovement();
            
            FGameplayTagContainer AttackTags;
            AttackTags.AddTag(FTTags::FTAbilities::Minion_Attack);
            
            // 주권 서버 하에서 확실하게 일반 공격 어빌리티 파이프라인을 사출 유도합니다.
            MyASC->TryActivateAbilitiesByTag(AttackTags);
        }
        else
        {
            // [적 추격 상태]: 사거리 밖의 타깃을 최단 경로 내비게이션으로 끝까지 추격하도록 컨트롤러에 기동 하달
            AIC->MoveToActor(BestTarget, AttackAcceptanceRadius, true, true, true);
        }
    }
    else
    {
        // 주위에 적이 없다면 타깃 시선 락을 클리어하고 라인 복귀 연산을 단행합니다.
        AIC->ClearFocus(EAIFocusPriority::Gameplay);

        AFT_MinionCharacterBase* MinionChar = Cast<AFT_MinionCharacterBase>(AvatarChar);
        if (MinionChar)
        {
            // 미니언 본체 헤더에 보관 중인 현재 타겟 웨이포인트 주소지 견인
            AFT_WayPoint* TargetWP = MinionChar->GetCurrentTargetWayPoint();
            if (TargetWP)
            {
                float DistanceToWP = FVector::Dist(MinionChar->GetActorLocation(), TargetWP->GetActorLocation());
                
                // 거점 도달 반경 검수 및 다음 연결 고리 웨이포인트 갱신 피드백
                if (DistanceToWP <= TargetWP->ArrivalRadius)
                {
                    AFT_WayPoint* NextWP = TargetWP->NextWayPoint;
                    MinionChar->SetCurrentTargetWayPoint(NextWP);
                    TargetWP = NextWP;
                }
                
                // 최종 유효 거점의 3D 공간 좌표 벡터 패킷을 순정 블랙보드 우체통에 주입 주사합니다.
                if (TargetWP)
                {
                    BB->SetValueAsVector(TEXT("LineWaypoint"), TargetWP->GetActorLocation());
                }
                else
                {
                    BB->ClearValue(TEXT("LineWaypoint"));
                }
            }
        }

        // 정비된 블랙보드 목적지 벡터를 인양하여 라인 순찰 전진 기동 지령 사출
        FVector NextLineLocation = BB->GetValueAsVector(TEXT("LineWaypoint"));
        if (!NextLineLocation.IsZero())
        {
            AIC->MoveToLocation(NextLineLocation, 50.0f, true, true, true);
        }
    }
}

AActor* UFT_Minion_Brain::InternalScanBestTarget(AFTCharacterBase* AvatarChar, const FGameplayTag& MyTeamTag)
{
    // 중복 연산 소각: 이제 AI 컨트롤러의 AIPerception이 실시간 정밀 스캔을 전담하므로, 이 구역은 가벼움을 위해 완전 공백 정지합니다.
    return nullptr;
}

void UFT_Minion_Brain::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 어빌리티 해제 소멸 시점에 안전하게 주기적 정기 AI 스캔 타이머 컴포넌트를 청정 소각 제거합니다.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AIExecuteTimerHandle);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}