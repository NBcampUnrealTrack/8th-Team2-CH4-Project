// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTMainMenu.generated.h"

class UButton;
class UTextBlock;
class UWidgetAnimation;
class UFTMainMenuWidget;

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFTMainMenu : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UButton* PlayButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PlayText;

	// C++ 클래스에서 사용자가 UMG 디자이너에서 만든 애니메이션을 바인딩합니다.
	UPROPERTY(Transient,meta = (BindWidgetAnim))
	UWidgetAnimation* HoverAnimation;
	
	// 메인 메뉴에 나타날 위젯
	UPROPERTY(meta = (BindWidget))
	UFTMainMenuWidget* WBP_MainMenuWidget;

	UFUNCTION()
	void OnPlayButtonHovered();

	UFUNCTION()
	void OnPlayButtonUnhovered();

	UFUNCTION()
	void OnPlayButtonClicked();
};
