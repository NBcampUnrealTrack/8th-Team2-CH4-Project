// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTCircularProgressWidget.generated.h"

class UImage;
class UTextBlock;
class UMaterialInstanceDynamic;

/**
 *	원형 프로그레스 게이지 위젯(WBP_CirculProgressBar 부모 클래스).
 *	GaugeBar(Image)의 머티리얼을 다이나믹 인스턴스로 변환해 Percent/Thickness 스칼라 파라미터를 조절하고,
 *	Second(TextBlock)로 중앙 텍스트를 표시한다.
 */
UCLASS()
class FIERYTALE_API UFTCircularProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 0~1로 클램프해 머티리얼의 "Percent" 스칼라 파라미터에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale|UI")
	void SetPercent(float NewPercent);

	// 머티리얼의 "Thickness" 스칼라 파라미터에 반영한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale|UI")
	void SetThickness(float NewThickness);

	// 중앙 텍스트(Second)를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale|UI")
	void SetCenterText(const FText& NewText);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GaugeBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Second;

private:
	// GaugeBar에 현재 지정된 머티리얼로부터 다이나믹 인스턴스를 1회 생성해 캐싱한다.
	void EnsureGaugeMID();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GaugeMID;
};
