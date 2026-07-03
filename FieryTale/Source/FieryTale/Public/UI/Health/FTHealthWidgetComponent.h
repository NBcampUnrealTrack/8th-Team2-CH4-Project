#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "FTHealthWidgetComponent.generated.h"

class UAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FIERYTALE_API UFTHealthWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	// 컴포넌트 기본 설정용 생성자
	UFTHealthWidgetComponent();

	// 위젯 생성 시점 명세 확장
	virtual void InitWidget() override;

	// 컴포넌트 활성화 수명주기 확장
	virtual void BeginPlay() override;

	// 월드 컴포넌트 등록 수명주기 확장
	virtual void OnRegister() override;

	// 프레임별 카메라 방향 정렬 틱 연산
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 외부 주입용 ASC 동기화 처리
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	// 소유 액터의 GAS 컴포넌트 역추적 바인딩
	void UpdateASCBinding();

	// 수동 좌표 정렬용 폴백 함수
	void AutoPositionWidget();

	// 블루프린트 패널에서 조절 가능한 카메라 방향 견인 거리 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	float PullDistance = 100.0f;

	// 블루프린트 패널에서 조절 가능한 수직 높이 오프셋 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	float AdditionalZOffset = 0.0f;
};