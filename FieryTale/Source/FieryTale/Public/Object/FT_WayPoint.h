// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FT_WayPoint.generated.h"

class USphereComponent;

/**
 * AOS 라인의 주요 거점 및 모퉁이를 마킹하고 체인 형태로 연결하는
 * 순정 웨이포인트 액터 클래스입니다.
 */
UCLASS()
class FIERYTALE_API AFT_WayPoint : public AActor
{
	GENERATED_BODY()
    
public:    
	AFT_WayPoint();

	/** 현재 거점에 도달했을 때 미니언이 다음으로 향해야 할 목적지 거점 포인터 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieryTale | Navigation")
	TObjectPtr<AFT_WayPoint> NextWayPoint;

	/** 개발 및 레벨 디자인 편의를 위한 거점 반경 범위 가시화용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieryTale | Navigation")
	float ArrivalRadius = 150.0f;

protected:
#if WITH_EDITOR
	// 💡 에디터 패키징 빌드 시 컴파일 파쇄를 막기 위한 양단 가드 매칭 완착
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private:
	/** 
	 * [레벨 디자인 픽스]: 에디터 뷰포트에서 미니언 도달 범위를 시각적으로 명확히 락인해 줄 가시화 헬퍼 컴포넌트입니다.
	 * 인게임 빌드(Shipping) 환경에서는 물리 연산 비용을 0으로 지우기 위해 콜리전을 원천 소각합니다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "FieryTale | Editor Only")
	TObjectPtr<USphereComponent> EditorVisualizerSphere;
};