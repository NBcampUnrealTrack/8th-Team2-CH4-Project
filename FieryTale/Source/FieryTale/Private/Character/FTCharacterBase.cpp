// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/FTTags.h"

AFTCharacterBase::AFTCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFTCharacterBase::Die(AController* KillerController)
{
	// Dead 태그로 중복 사망 처리를 차단하고, 어빌리티 발동 차단(ActivationBlockedTags)과
	// 클라이언트 상태 표시(OnDeadTagChanged)를 위해 서버·클라 양쪽에 태그를 전파한다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead))
		{
			return;
		}
		// UE 5.8 통합 API: TagAndCountToAll로 서버·모든 클라이언트에 태그 전파
		ASC->AddLooseGameplayTag(FTTags::FTStates::Core::Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// bUseControllerRotationYaw가 true인 채로 남아있으면 사망 몽타주 재생 중에도
	// 카메라(컨트롤러) Yaw를 따라 캡슐이 계속 회전해 몽타주가 도는 것처럼 보인다.
	bUseControllerRotationYaw = false;

	OnCharacterDied.Broadcast(this, KillerController);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();

		FGameplayEventData EventData;
		EventData.EventTag = FTTags::Events::CharacterDeath;
		EventData.Instigator = this;

		if (ASC->HandleGameplayEvent(FTTags::Events::CharacterDeath, &EventData) == 0)
		{
			FinishDying();
		}
	}
	else
	{
		FinishDying();
	}
}

void AFTCharacterBase::FinishDying()
{
}


void AFTCharacterBase::InitGAS(UAbilitySystemComponent* ASC)
{
	if (!ASC || !HasAuthority())
	{
		return;
	}

	
}
