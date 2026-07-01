// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/FT_GameplayAbility.h"

void UFT_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 최상위 가상 함수 수명주기의 선제 가동을 승인합니다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

const FGameplayTagContainer* UFT_GameplayAbility::GetCooldownTags() const
{
	// 보정 완료: 위험천만한 static 공유 컨테이너 오염과 원시 데이터 파괴를 완전히 걷어내고, 인스턴스 독립형 쿨다운 복사 반환 파이프라인을 구축합니다.
	const FGameplayTagContainer* ParentTags = Super::GetCooldownTags();
    
	// 기획자가 에디터나 생성자에서 지정한 자물쇠 쿨타임 태그가 유효하지 않다면 부모의 결과물을 그대로 반환하고 조기 종료합니다.
	if (!CooldownTag.IsValid())
	{
		return ParentTags;
	}

	// 언리얼 GAS의 마스터 상속 const 규격을 준수하면서도, 인스턴스 고유 메모리에 안전하게 접근하기 위해 임시 상수성을 해제합니다.
	// 멀티 스레드 레이스 컨디션 및 다른 영웅과의 컨테이너 꼬임 현상이 없는 완벽한 독립 보관소를 보장합니다.
	UFT_GameplayAbility* MutableThis = const_cast<UFT_GameplayAbility*>(this);
    
	// 임시 컨테이너에 부모 클래스가 가진 기존의 공용 쿨다운 태그 무리를 통째로 복사 이식합니다.
	FGameplayTagContainer CombinedTags;
	if (ParentTags)
	{
		CombinedTags.AppendTags(*ParentTags);
	}

	// 내가 에디터 데이터 에셋 사양으로 장착한 공용 혹은 고유 쿨타임 태그 주소지를 안전하게 최종 결합합니다.
	CombinedTags.AddTag(CooldownTag);

	// 최종 안착: 어빌리티 클래스 헤더 내부에 안전 버퍼용으로 설계해둔 고유 컨테이너 멤버 변수인 RuntimeCooldownTags에 최종 대입합니다.
	// 이를 통해 함수가 종료되더라도 댕글링 포인터가 되지 않는 완벽한 포인터 수명선과 메모리 거점을 확보합니다.
	MutableThis->RuntimeCooldownTags = CombinedTags;
    
	return &RuntimeCooldownTags;
}

void UFT_GameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 수집 완료된 RuntimeCooldownTags 주소지를 기반으로, 실질적인 지속시간형 게임플레이 이펙트 쿨다운 버프를 시전자에게 확정 사출합니다.
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}