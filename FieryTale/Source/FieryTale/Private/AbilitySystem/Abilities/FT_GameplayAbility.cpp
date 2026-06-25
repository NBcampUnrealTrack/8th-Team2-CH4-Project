// Fill out your copyright notice in the Description page of Project Settings.


#include "FieryTale/Public/AbilitySystem/Abilities/FT_GameplayAbility.h"

#include "Engine/Engine.h"

void UFT_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                          const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (bDrawDebugs && IsValid(GEngine))
	{
		GEngine->AddOnScreenDebugMessage(-1,3.f,FColor::Cyan,FString::Printf(TEXT("%s Activated"), *GetName()));
	}
}
