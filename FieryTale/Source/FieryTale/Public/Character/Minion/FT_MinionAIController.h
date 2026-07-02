// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "FT_MinionAIController.generated.h"

class UBlackboardData;
class UBehaviorTree;

/**
 * 전술 제어권을 GAS(UFT_Minion_Brain)로 이관하고,
 * 오직 블랙보드 장부 관리 및 기계적 무빙 명령만 충직하게 대행하는 초경량 미니언 AI 컨트롤러입니다.
 */
UCLASS()
class FIERYTALE_API AFT_MinionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFT_MinionAIController();

protected:
	/** 미니언 Pawn이 빙의(Possess)될 때 블랙보드 장부를 개통하는 관문 */
	virtual void OnPossess(APawn* InPawn) override;

	// =========================================================================
	// [기획 및 에디터 매핑 자산]
	// =========================================================================
	/** 미니언이 사용할 순정 블랙보드 에셋 (LineWaypoint 벡터 키가 포함된 장부) */
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale | AI")
	TObjectPtr<UBlackboardData> MinionBlackboardAsset;
};