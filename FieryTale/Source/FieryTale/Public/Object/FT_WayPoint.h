// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FT_WayPoint.generated.h"

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
	float ArrivalRadius;
};