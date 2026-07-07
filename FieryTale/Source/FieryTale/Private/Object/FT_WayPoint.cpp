// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/FT_WayPoint.h"
#include "Components/SphereComponent.h"

AFT_WayPoint::AFT_WayPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1단계: 레벨 디자인 전용 가시화 인프라 타설
	EditorVisualizerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EditorVisualizerSphere"));
	SetRootComponent(EditorVisualizerSphere);

	// [중요 가드선]: 이 컴포넌트는 오직 시각적 가이드 라인일 뿐이므로, 
	// 미니언의 투사체나 이동 캡슐을 가로막는 대형 참사를 막기 위해 콜리전을 완벽히 꺼버립니다.
	EditorVisualizerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorVisualizerSphere->SetCollisionProfileName(TEXT("NoCollision"));
    
	// 기저 사양으로 설정된 반경 수치를 컴포넌트에 동기화 투사합니다.
	EditorVisualizerSphere->SetSphereRadius(ArrivalRadius);
    
	// 인게임 플레이 도중에는 화면에서 보이지 않도록 순정 하이드 처리합니다.
	EditorVisualizerSphere->SetHiddenInGame(true);
}

#if WITH_EDITOR
void AFT_WayPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 디자이너가 마우스로 ArrivalRadius 값을 긁어 변경할 때 실시간으로 구체 크기가 변하도록 연동합니다.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AFT_WayPoint, ArrivalRadius))
	{
		if (EditorVisualizerSphere)
		{
			EditorVisualizerSphere->SetSphereRadius(ArrivalRadius);
		}
	}
}
#endif