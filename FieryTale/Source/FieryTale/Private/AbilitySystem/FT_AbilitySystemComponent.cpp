// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTags/FTTags.h"

void UFT_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
    // 서버 환경에서 캐릭터에게 특정 스킬이나 패시브 소유권이 최초로 부여되는 시점에 호출됩니다.
    Super::OnGiveAbility(AbilitySpec);
    
    // 자동 활성화 검증: 새로 부여된 능력이 상시 발동되어야 하는 패시브 계열인지 확인합니다.
    HandleAutoActivatedAbility(AbilitySpec);
}

void UFT_AbilitySystemComponent::OnRep_ActivateAbilities()
{
    // 멀티플레이어 환경에서 클라이언트가 서버로부터 능력 리스트 정보를 복제받을 때 호출됩니다.
    Super::OnRep_ActivateAbilities();
    
    // 어빌리티 리스트 락: 리스트 순회 중 배열이 수정되어 발생하는 크래시를 방지합니다.
    FScopedAbilityListLock ActiveScopesLock(*this);
    
    // 패시브 스킬 누락을 방지하기 위해 전체 리스트를 순회하며 자동 활성화를 검증합니다.
    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
       HandleAutoActivatedAbility(AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    // 서버 권한을 확인합니다.
    if (!IsOwnerActorAuthoritative())
    {
       return;
    }
    
    // 등록된 어빌리티 클래스의 레벨을 지정된 수치로 설정합니다.
    if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
    {
       AbilitySpec->Level = Level;
       
       // 변경된 레벨을 클라이언트와 동기화하기 위해 Dirty 마킹을 수행합니다.
       MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::AddToAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    // 스킬 포인트 투자 시 누적 연산을 수행합니다.
    // 서버 권한을 확인합니다.
    if (!IsOwnerActorAuthoritative())
    {
       return;
    }
    
    // 기존 스킬 레벨에 지정된 수치만큼 누적 합산을 수행합니다.
    if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
    {
       AbilitySpec->Level += Level;
       
       // 변경 사항 동기화
       MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
    // 전달받은 스킬 인스턴스의 유효성을 검증합니다.
    if (!IsValid(AbilitySpec.Ability))
    {
       return;
    }
    
    // 서버 권한을 확인하여 클라이언트 단독 시전으로 인한 문제를 방지합니다.
    if (!IsOwnerActorAuthoritative())
    {
        return;
    }

    // 태그를 검사하여 상시 활성화(ActivateOnGiven) 대상을 식별합니다.
    if (AbilitySpec.Ability->GetAssetTags().HasTagExact(FTTags::FTAbilities::ActivateOnGiven))
    {
        // 조건을 충족한 패시브 어빌리티를 서버 권한 환경에서 활성화합니다.
        TryActivateAbility(AbilitySpec.Handle);
    }
}