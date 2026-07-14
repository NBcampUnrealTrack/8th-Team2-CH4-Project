// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "FT_Minion_Brain.generated.h"

class AAIController;
class AFTCharacterBase;

/**
 * 미니언의 AI 로직(시야 스캔, 피아 식별, 이동 및 공격)을 담당하는 어빌리티입니다.
 */
UCLASS()
class FIERYTALE_API UFT_Minion_Brain : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UFT_Minion_Brain();

protected:
    /** 어빌리티가 활성화될 때 호출됩니다. */
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    /** 어빌리티가 종료될 때 호출됩니다. */
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // =========================================================================
    // AI 설정
    // =========================================================================
    
    /** AI 로직 업데이트 주기 (초 단위) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI | Config")
    float ScanInterval = 0.5f;

    /** 시야 및 탐색 반경 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI | Config")
    float ScanRange = 800.0f;

    /** 공격 허용 반경 (이 반경 내에 들어오면 공격을 시도합니다) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI | Config")
    float AttackAcceptanceRadius = 120.0f;

private:
    /** AI 로직을 실행합니다. */
    void ExecuteAILogic();

    /** 주변 스캔 후 최적의 타깃을 반환합니다. */
    AActor* InternalScanBestTarget(AFTCharacterBase* AvatarChar, const FGameplayTag& MyTeamTag);

    /** AI 로직 실행 타이머 핸들 */
    FTimerHandle AIExecuteTimerHandle;
    
    /** 이전 틱에서 기절 상태였는지 여부 */
    bool bWasStunnedLastTick = false;
};