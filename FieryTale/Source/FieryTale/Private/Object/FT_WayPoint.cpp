// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_WayPoint.h"
#include "Components/SphereComponent.h"

AFT_WayPoint::AFT_WayPoint()
	: ArrivalRadius(150.0f) // [C++ 정순 초기화 완착]: 변수 주소지에 기저 도달 반경(150유닛)을 선제 락인합니다.
{
	PrimaryActorTick.bCanEverTick = false;

	// 1단계: 레벨 디자인 전용 가시화 인프라 타설
	EditorVisualizerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EditorVisualizerSphere"));
	SetRootComponent(EditorVisualizerSphere);

	// [중요 가드선]: 투사체나 이동 캡슐의 경로를 방해하지 않도록 콜리전을 전면 소각합니다.
	EditorVisualizerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorVisualizerSphere->SetCollisionProfileName(TEXT("NoCollision"));
    
	// 초기화가 완료된 순정 기저 수치를 컴포넌트에 안전하게 동기화 투사합니다.
	EditorVisualizerSphere->SetSphereRadius(ArrivalRadius);
    
	// 인게임 플레이 도중에는 화면에서 보이지 않도록 순정 하이드 처리합니다.
	EditorVisualizerSphere->SetHiddenInGame(true);
}

#if WITH_EDITOR
void AFT_WayPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 디자이너가 에디터에서 ArrivalRadius 값을 변경할 때 실시간으로 뷰포트 내 구체 크기가 변하도록 연동합니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AFT_WayPoint, ArrivalRadius))
	{
		if (EditorVisualizerSphere)
		{
			EditorVisualizerSphere->SetSphereRadius(ArrivalRadius);
		}
	}
}
#endif