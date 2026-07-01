// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTags/FTTags.h"

void UFT_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
    // 서버 환경에서 캐릭터에게 특정 스킬이나 패시브 소유권이 최초로 주입되는 시점에 가동됩니다.
    Super::OnGiveAbility(AbilitySpec);
    
    // 자동 활성화 검증: 새로 부여된 능력이 상시 발동되어야 하는 패시브 계열인지 정밀 필터링을 수행합니다.
    HandleAutoActivatedAbility(AbilitySpec);
}

void UFT_AbilitySystemComponent::OnRep_ActivateAbilities()
{
    // 멀티플레이어 환경에서 클라이언트가 서버로부터 능력 리스트 정보를 동기화 패킷으로 복제받을 때 격발됩니다.
    Super::OnRep_ActivateAbilities();
    
    // 능력 리스트 락 가동: 복제 루프 연산이 도는 도중에 네트워크 레이스 컨디션으로 인해 배열 구조가 파괴되며 터지는 런타임 크래시를 원천 방지합니다.
    FScopedAbilityListLock ActiveScopesLock(*this);
    
    // 레이트 조인이나 네트워크 재접속 시에도 패시브 스킬이 누락되는 좀비 현상을 방지하기 위해 전체 리스트를 순회하며 재검증 라인을 가동합니다.
    for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
    {
       HandleAutoActivatedAbility(AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    // 멀티플레이어 환경에서 클라이언트의 메모리 변조 핵 치트를 원천 차단하기 위해 오직 서버 권한을 가진 상태인지 검증합니다.
    if (!IsOwnerActorAuthoritative())
    {
       return;
    }
    
    // 등록된 어빌리티 클래스 정보를 기반으로 런타임 스펙을 탐색하여 기획 지정 레벨 수치를 강제 주입합니다.
    if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
    {
       AbilitySpec->Level = Level;
       
       // 중요: 변경된 레벨 데이터를 모든 클라이언트에게 즉시 실시간 복제 전파하도록 직렬화 더티 마킹 배관을 격발합니다.
       MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::AddToAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    // AOS 인게임 규칙: 레벨업 이후 플레이어가 1번 평타나 2번 스킬 포인트를 투자했을 때 누적 연산을 수행하는 인터페이스입니다.
    // 변조 방지를 위해 오직 권한을 가진 서버 단독 연산선으로 제어합니다.
    if (!IsOwnerActorAuthoritative())
    {
       return;
    }
    
    // 기존에 영웅이 보유하고 있던 스킬 레벨에 투자한 포인트 수치만큼 복사 오차 없이 안전하게 누적 합산을 수행합니다.
    if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromClass(AbilityClass))
    {
       AbilitySpec->Level += Level;
       
       // 변경 사항 더티 직렬화 전파 격발
       MarkAbilitySpecDirty(*AbilitySpec);
    }
}

void UFT_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
    // 전달받은 스킬 내부 인스턴스가 유효한 상태인지 선제 검사하여 포인터 폭사를 방어합니다.
    if (!IsValid(AbilitySpec.Ability))
    {
       return;
    }
    
    // 서버 권한 검증: 클라이언트 단독 시전으로 인해 발생하는 무한 동기화 재귀 루프와 패킷 폭사를 차단하기 위해 오직 Authoritative 상태에서만 통제합니다.
    if (!IsOwnerActorAuthoritative())
    {
        return;
    }

    // 데이터 에셋 세팅 가이드: 기획자분들이 상시 유지되는 패시브 스킬 블루프린트를 제작할 때는 에셋 태그 슬롯에 ActivateOnGiven 태그를 명확히 바인딩해야 합니다.
    // 컨테이너 직통 정확성 조회를 가동하여 연산 부하 없이 상시 활성화 대상을 골라냅니다.
    if (AbilitySpec.Ability->GetAssetTags().HasTagExact(FTTags::FTAbilities::ActivateOnGiven))
    {
        // 조건을 충족한 패시브 능력을 시스템 내부 수명 주기에 맞춰 서버 소유권 하에 강제로 안전하게 격발 구동시킵니다.
        TryActivateAbility(AbilitySpec.Handle);
    }
}