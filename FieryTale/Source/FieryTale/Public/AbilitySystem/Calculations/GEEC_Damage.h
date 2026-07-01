// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GEEC_Damage.generated.h"

/**
 * FieryTale 전장의 진영 피아식별 및 대미지 가감 연산을 총괄하는 핵심 네이티브 계산기
 */
UCLASS()
class FIERYTALE_API UGEEC_Damage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UGEEC_Damage();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& ExecutionOutputs) const override;
};