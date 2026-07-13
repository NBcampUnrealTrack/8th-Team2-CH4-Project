// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "FT_MinionAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * 비헤비어 트리와 블랙보드를 완전히 배제하고, 
 * 퍼셉션 센서와 순수 C++ 포인터 참조로 미니언 AI 무빙을 제어하는 마스터 컨트롤러입니다.
 */
UCLASS()
class FIERYTALE_API AFT_MinionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFT_MinionAIController();
	void BeginPlay();

	/** 💡 [C++ 직격 인양 게터/세터]: 브레인 어빌리티가 블랙보드 없이 타깃을 실시간 참조하도록 개통합니다. */
	FORCEINLINE AActor* GetTargetEnemyActor() const { return TargetEnemyActor; }
	FORCEINLINE void SetTargetEnemyActor(AActor* InTarget) { TargetEnemyActor = InTarget; }
	void ClearTargetEnemyActor() { TargetEnemyActor = nullptr; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	// AIPerception 시각 감지 신호를 수신하여 타깃 포인터를 실시간 동기화하는 델리게이트 함수입니다.
	UFUNCTION()
	virtual void OnMinionPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// 순정 AI 색적 컴포넌트 슬롯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | AI")
	TObjectPtr<UAIPerceptionComponent> MinionPerceptionComponent;

	// 시각 센서 정밀 속성 설정 데이터 슬롯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

private:
	/** 💡 [블랙보드 대체 핵심 변수]: 실시간 락온된 적대 타깃 액터의 다이렉트 주권 포인터입니다. */
	UPROPERTY()
	TObjectPtr<AActor> TargetEnemyActor;
};