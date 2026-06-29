// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"

void UFT_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
    Super::OnGiveAbility(AbilitySpec);
    
    // 서버 단에서 어빌리티가 캐릭터에게 부여되는 시점에 자동 활성화 여부를 판단합니다
    HandleAutoActivatedAbility(AbilitySpec);
}

void UFT_AbilitySystemComponent::OnRep_ActivateAbilities()
{
    Super::OnRep_ActivateAbilities();
    
    // 능력 리스트 락을 통해 루프 도중 배열 구조가 변경되는 크래시를 방지합니다
    FScopedAbilityListLock ActiveScopesLock(*this);
    
    // 멀티플레이 네트워크 환경에서 클라이언트가 어빌리티 정보를 복제받을 때 자동 활성화를 재검증합니다
    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
       HandleAutoActivatedAbility(AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    // 멀티플레이어 환경에서 치트를 방지하기 위해 권한이 없는 클라이언트의 요청은 차단합니다
    if (IsValid(GetAvatarActor()) && !GetAvatarActor()->HasAuthority())
    {
       return;
    }
    
    // 클래스 정보를 기반으로 등록된 어빌리티 스펙을 찾아 레벨을 강제 지정합니다
    if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
    {
       AbilitySpec->Level = Level;
       
       // 레벨 정보가 변경되었음을 클라이언트에 전파하기 위해 더티 마킹을 수행합니다
       MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::AddToAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    // AOS 레벨업에 따른 스킬 포인트 투자 시 서버 권한 검증을 거칩니다
    if (IsValid(GetAvatarActor()) && !GetAvatarActor()->HasAuthority())
    {
       return;
    }
    
    // 기존 어빌리티 레벨 수치에 누적 합산을 수행합니다
    if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
    {
       AbilitySpec->Level += Level;
       MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
    if (!IsValid(AbilitySpec.Ability))
    {
       return;
    }
    
    // 스킬 에셋에 지정된 태그 목록을 순회하여 자동 활성화 패시브 스킬인지 필터링합니다
    for (const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
    {
       if (Tag.MatchesTagExact(FTTags::FTAbilities::ActivateOnGiven))
       {
          // 조건 충족 시 해당 어빌리티를 강제로 시전 루프에 진입시킵니다
          TryActivateAbility(AbilitySpec.Handle); 
          return;
       }
    }
}