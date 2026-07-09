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

	OnCharacterDied.Broadcast(this, KillerController);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
	}

	if (DeathMontage)
	{	
		// TODO:: Die가 서버에서 실행되는 코드 -> 
		// 현재 방식으로는 DeathMontage가 클라이언트에 복제 / Multicast되지 않는 문제가 있음. 따라서 이 방식으로 쓰기엔 부적절함
		// const float Duration = PlayAnimMontage(DeathMontage);
		
		/*if (Duration <= 0.0f)
		{
			FinishDying();
		}*/
		
		FinishDying();
	}
	else
	{
		FinishDying();
	}
}

void AFTCharacterBase::FinishDying()
{
	// TODO:: Destroy()로 처리하기는 애매해서 우선 공백으로 둠
}


void AFTCharacterBase::InitGAS(UAbilitySystemComponent* ASC)
{
	if (!ASC || !HasAuthority())
	{
		return;
	}

	
}
