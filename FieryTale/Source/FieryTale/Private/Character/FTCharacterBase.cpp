// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

AFTCharacterBase::AFTCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFTCharacterBase::Die()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	OnCharacterDied.Broadcast(this);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
	}

	if (DeathMontage)
	{
		const float Duration = PlayAnimMontage(DeathMontage);
		if (Duration <= 0.0f)
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
	// TODO:: Destroy()로 처리하기는 애매해서 우선 공백으로 둠
}


void AFTCharacterBase::InitGAS(UAbilitySystemComponent* ASC)
{
	if (!ASC || !HasAuthority())
	{
		return;
	}

	
}
