// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTDeathWidget.generated.h"

class UFTCircularProgressWidget;

/**
 *	사망 시 표시되는 오버레이 위젯. 소유 PlayerController의 RespawnAvailableTime(서버 시각 기준,
 *	OwnerOnly로 복제됨)을 NativeTick에서 폴링해 남은 부활 시간을 갱신한다.
 *
 *	BP에서 이 클래스를 상속해 RespawnGauge(UFTCircularProgressWidget, BindWidget)를 지정한다.
 */
UCLASS()
class FIERYTALE_API UFTDeathWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	UFTCircularProgressWidget* RespawnGauge;
};
