// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FTHUDLayoutSubsystem.h"

#include "UI/FTMovableWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

void UFTHUDLayoutSubsystem::RegisterMovableWidget(UFTMovableWidget* Widget)
{
	if (!Widget || Widgets.Contains(Widget))
	{	//	중복등록 하지않기
		return;
	}

	Widgets.Add(Widget);

	//	등록 즉시 현재 편집 모드 상태에 맞춰 핸들 표시
	Widget->SetHandleVisible(bEditMode);
}

void UFTHUDLayoutSubsystem::UnregisterMovableWidget(UFTMovableWidget* Widget)
{
	Widgets.Remove(Widget);
}

void UFTHUDLayoutSubsystem::SetEditMode(bool bEnable)
{
	if (bEditMode == bEnable)
	{
		return;
	}

	bEditMode = bEnable;

	for (UFTMovableWidget* Widget : Widgets)
	{
		if (Widget)
		{
			Widget->SetHandleVisible(bEditMode);
		}
	}

	ApplyInputMode(bEditMode);

	//	TODO(5단계): 편집 종료(bEditMode == false) 시 전체 레이아웃 저장
}

void UFTHUDLayoutSubsystem::ApplyInputMode(bool bEditing)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	APlayerController* PC = LocalPlayer->GetPlayerController(LocalPlayer->GetWorld());
	if (!PC)
	{
		return;
	}

	if (bEditing)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}
