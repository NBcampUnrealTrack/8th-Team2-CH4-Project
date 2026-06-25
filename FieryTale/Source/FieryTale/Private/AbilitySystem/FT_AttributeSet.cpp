// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameplayTags/FTTags.h"

UFT_AttributeSet::UFT_AttributeSet()
	: Health(180.f)       // 모든 영웅 기본 스탯 HP 180 반영
	, MaxHealth(180.f)    // 최대 체력 상한선 180 설정
	, Shield(0.f)         // 초기 보호막은 0
	, MoveSpeed(600.f)    // 기본 이동속도 세팅
	, AttackPower(1.0f)   // 기본 공격력 배율 100%
	, UltimateGauge(0.f)  // 궁극기 게이지 0% 시작
{
}

void UFT_AttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 데디케이트 서버에서 클라이언트로 동기화할 속성 조건 등록
	DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFT_AttributeSet, UltimateGauge, COND_None, REPNOTIFY_Always);
}

void UFT_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 버프나 디버프, 스킬 조작 등으로 값이 '바뀌기 직전' 범위 한계값 캡슐화 처리
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetUltimateGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		// 이동속도가 마이너스가 되거나 비정상적으로 느려지는 현상 방지 (최소 0)
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UFT_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 피해량 계산(Execution)이나 힐 이펙트가 '완전히 적용된 직후' 처리
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// 변경된 실제 Health 값이 0 이하로 떨어졌을 때만 사망 파이프라인 작동
		if (GetHealth() <= 0.0f)
		{
			FGameplayEventData Payload;
			Payload.Instigator = Data.Target.GetAvatarActor();
			
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Data.EffectSpec.GetEffectContext().GetInstigator(), FTTags::Events::KillScored, Payload);
			// TODO: 데디케이트 서버용 고정 10초 리스폰 타이머 및 PlayerState 데스 카운트 핸들러 격발 규칙 연동
		}
	}
}

// --- 네트워크 복제(Replication) 핸들러 구현부 ---
void UFT_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, Health, OldHealth);
}

void UFT_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MaxHealth, OldMaxHealth);
}

void UFT_AttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, Shield, OldShield);
}

void UFT_AttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, MoveSpeed, OldMoveSpeed);
}

void UFT_AttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, AttackPower, OldAttackPower);
}

void UFT_AttributeSet::OnRep_UltimateGauge(const FGameplayAttributeData& OldUltimateGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFT_AttributeSet, UltimateGauge, OldUltimateGauge);
}