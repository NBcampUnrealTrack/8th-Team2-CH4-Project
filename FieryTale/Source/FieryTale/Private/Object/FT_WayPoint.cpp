// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_WayPoint.h"
#include "Components/BillboardComponent.h"

AFT_WayPoint::AFT_WayPoint()
	: ArrivalRadius(150.0f) // 미니언이 이 반경 내에 진입하면 거점 도달로 판정
{
	// 레벨 에디터 상에서 디자이너가 거점을 쉽게 알아볼 수 있도록 순정 빌보드 컴포넌트 장착
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		RootComponent = Billboard;
	}

	PrimaryActorTick.bCanEverTick = false;
}