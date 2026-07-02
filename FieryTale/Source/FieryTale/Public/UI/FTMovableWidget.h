// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTMovableWidget.generated.h"

class UBorder;
class UNamedSlot;
class UCanvasPanelSlot;
class UPanelWidget;
class UFTHUDLayoutSubsystem;

/**
 *	HUD에서 플레이어가 자유롭게 위치를 옮길 수 있는 위젯.
 *	- 반드시 Canvas Panel의 자식으로 배치되어야 위치 이동이 가능하다.
 *	- DragHandleArea(Border) 위를 잡고 끌면 이동한다. (편집 모드일 때만)
 */
UCLASS()
class FIERYTALE_API UFTMovableWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FName GetWidgetID() const { return WidgetID; }

	//	편집 모드 On/Off 시 Subsystem이 호출 — 드래그 핸들 표시/숨김
	void SetHandleVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	//	저장/복원용 키 — 각 인스턴스마다 고유하게 지정할 것
	UPROPERTY(EditAnywhere, Category = "FieryTale")
	FName WidgetID;

	//	true면 전역 편집 모드(F11)와 무관하게 이 위젯은 항상 핸들이 표시되고 드래그 이동이 가능하다.
	//	상위 WBP 디자이너에서 배치된 인스턴스별로 지정한다.
	//	(커서/입력 모드는 전환하지 않으므로 UI 입력이 가능한 화면에서 사용할 것)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieryTale")
	bool bAlwaysEditMode = false;

	//	이 영역을 잡고 드래그한다. Visibility는 Visible 이어야 입력을 받는다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> DragHandleArea;

	//	실제 내용(미니맵/스킬바 등)을 끼워 넣는 슬롯
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UNamedSlot> ContentSlot;

private:
	UFTHUDLayoutSubsystem* GetLayoutSubsystem() const;

	//	자신이 속한 Canvas 슬롯 (위치 변경 대상)
	UCanvasPanelSlot* GetCanvasSlot() const;

	//	부모 Canvas의 지오메트리 — 화면 좌표를 Canvas 로컬 좌표로 변환하는 기준
	FGeometry GetParentGeometry() const;

	bool bIsDragging = false;

	//	드래그 시작 시 "위젯 좌상단 ~ 커서" 사이 간격 (Canvas 로컬 좌표)
	FVector2D GrabOffset = FVector2D::ZeroVector;
};
