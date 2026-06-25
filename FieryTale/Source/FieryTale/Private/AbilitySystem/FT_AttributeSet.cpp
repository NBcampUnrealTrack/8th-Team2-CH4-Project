// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayEffectExtension.h"
#include "GameplayTags/FTTags.h"
#include "Net/UnrealNetwork.h"

/** 
 * [GetLifetimeReplicatedProps]
 * 어떤 프로퍼티를 네트워크를 통해 복제할지, 그리고 어떤 조건에서 복제할지 결정합니다.
 */
void UFT_AttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxMana, COND_None, REPNOTIFY_Always);
	
	DOREPLIFETIME(ThisClass, bAttributeInitialized);
}

void UFT_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	if (Data.EvaluatedData.Attribute == GetHealthAttribute() && GetHealth() <= 0.f)
	{
		FGameplayEventData Payload;
		Payload.Instigator = Data.Target.GetAvatarActor();
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Data.EffectSpec.GetEffectContext().GetInstigator(), FTTags::Events::KillScored, Payload);
	}
}

void UFT_AttributeSet::OnRep_AttributeInitialized()
{
	if (bAttributeInitialized)
	{
		OnAttributeInitialized.Broadcast();
	}
}

/** 
 * 아래 OnRep 함수들은 서버로부터 새로운 수치를 받았을 때 
 * 클라이언트의 ASC(Ability System Component)에게 "수치가 변했으니 확인해라!"라고 알려주는 역할을 합니다.
 */
// GAMEPLAYATTRIBUTE_REPNOTIFY 매크로는 내부적으로 수치 변경 전/후 처리를 담당합니다.
void UFT_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Health, OldValue);
}

void UFT_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxHealth, OldValue);
}

void UFT_AttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Mana, OldValue);
}

void UFT_AttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxMana, OldValue);
}
