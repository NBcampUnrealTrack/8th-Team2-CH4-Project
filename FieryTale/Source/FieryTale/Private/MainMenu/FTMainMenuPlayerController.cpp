// Fill out your copyright notice in the Description page of Project Settings.


#include "MainMenu/FTMainMenuPlayerController.h"
#include "MainMenu/FTMainMenuWidget.h"

void AFTMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어일 때만 생성
	if (IsLocalPlayerController() && MainMenuWidgetClass)
	{
		UFTMainMenuWidget* MainMenuWidget = CreateWidget<UFTMainMenuWidget>(this, MainMenuWidgetClass);
		if (MainMenuWidget)
		{
			MainMenuWidget->AddToViewport();

			bShowMouseCursor = true;

			// UI 입력 모드로 설정
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(MainMenuWidget->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputModeData);
		}
	}
}
