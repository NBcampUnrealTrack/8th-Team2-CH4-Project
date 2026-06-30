// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "FTHUDLayoutSubsystem.generated.h"

class UFTMovableWidget;

/**
 *	Movable 위젯들을 관리하는 서브시스템.
 *	- 위젯 등록/해제
 *	- HUD 편집 모드 토글 (드래그 핸들 표시 + 입력 모드 전환)
 *	- (예정) 위치 저장/복원
 */
UCLASS()
class FIERYTALE_API UFTHUDLayoutSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	void RegisterMovableWidget(UFTMovableWidget* Widget);
	void UnregisterMovableWidget(UFTMovableWidget* Widget);

	//	편집 모드 전환 — 모든 등록 위젯의 핸들 표시/숨김 + 입력 모드 전환
	void SetEditMode(bool bEnable);
	void ToggleEditMode() { SetEditMode(!bEditMode); }
	bool IsEditMode() const { return bEditMode; }

private:
	//	편집 여부에 따라 PlayerController 입력 모드/커서 전환
	void ApplyInputMode(bool bEditing);

	UPROPERTY()
	TArray<TObjectPtr<UFTMovableWidget>> Widgets;

	bool bEditMode = false;
};
