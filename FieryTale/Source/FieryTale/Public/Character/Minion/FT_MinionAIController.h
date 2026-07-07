// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "FT_MinionAIController.generated.h"

class UBlackboardData;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UCLASS()
class FIERYTALE_API AFT_MinionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFT_MinionAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;

	// AIPerception 시각 감지 신호를 수신하여 블랙보드 장부를 실시간 동기화하는 델리게이트 함수입니다.
	UFUNCTION()
	virtual void OnMinionPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// [블랙보드 에셋 슬롯]
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | AI")
	TObjectPtr<UBlackboardData> MinionBlackboardAsset;

	// [순정 AI 색적 컴포넌트 슬롯 - 네이밍 상속 충돌 버그 보수 완착]
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | AI")
	TObjectPtr<UAIPerceptionComponent> MinionPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
};