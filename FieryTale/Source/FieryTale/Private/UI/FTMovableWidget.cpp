// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FTMovableWidget.h"

#include "UI/FTHUDLayoutSubsystem.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Engine/LocalPlayer.h"

void UFTMovableWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//	NativeConstruct 시점에는 부모 패널에 추가되어 Slot이 유효하다.
	if (UFTHUDLayoutSubsystem* Subsystem = GetLayoutSubsystem())
	{
		Subsystem->RegisterMovableWidget(this);
	}
}

void UFTMovableWidget::NativeDestruct()
{
	if (UFTHUDLayoutSubsystem* Subsystem = GetLayoutSubsystem())
	{
		Subsystem->UnregisterMovableWidget(this);
	}

	Super::NativeDestruct();
}

FReply UFTMovableWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	//	편집 모드가 아니면 드래그하지 않는다.
	UFTHUDLayoutSubsystem* Subsystem = GetLayoutSubsystem();
	if (!Subsystem || !Subsystem->IsEditMode())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	//	왼쪽 버튼이 핸들 영역 위에서 눌렸을 때만 시작
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (!DragHandleArea
		|| !DragHandleArea->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	UCanvasPanelSlot* CanvasSlot = GetCanvasSlot();
	if (!CanvasSlot)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bIsDragging = true;

	//	커서(화면 좌표)를 부모 Canvas 로컬 좌표로 변환한 뒤, 현재 위치와의 차이를 저장
	const FVector2D LocalCursor = GetParentGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	GrabOffset = LocalCursor - CanvasSlot->GetPosition();

	//	캡처해야 커서가 핸들 밖으로 나가도 Move 이벤트를 계속 받는다.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UFTMovableWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsDragging)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	UCanvasPanelSlot* CanvasSlot = GetCanvasSlot();
	if (!CanvasSlot)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D LocalCursor = GetParentGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	CanvasSlot->SetPosition(LocalCursor - GrabOffset);

	return FReply::Handled();
}

FReply UFTMovableWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsDragging)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	bIsDragging = false;

	//	TODO(5단계): Subsystem에 위치 변경을 통보해 SaveGame으로 저장
	return FReply::Handled().ReleaseMouseCapture();
}

void UFTMovableWidget::SetHandleVisible(bool bVisible)
{
	if (!DragHandleArea)
	{
		return;
	}

	//	편집 OFF 시 Collapsed로 두어 입력을 가로채지 않게 한다.
	DragHandleArea->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

UFTHUDLayoutSubsystem* UFTMovableWidget::GetLayoutSubsystem() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UFTHUDLayoutSubsystem>() : nullptr;
}

UCanvasPanelSlot* UFTMovableWidget::GetCanvasSlot() const
{
	//	UWidget::Slot은 부모 패널에 추가될 때 설정된다. Canvas가 아니면 nullptr.
	return Cast<UCanvasPanelSlot>(Slot);
}

FGeometry UFTMovableWidget::GetParentGeometry() const
{
	if (UPanelWidget* Parent = GetParent())
	{
		return Parent->GetCachedGeometry();
	}

	//	부모를 못 찾으면 자기 지오메트리로 폴백 (정상 배치 시엔 도달하지 않음)
	return GetCachedGeometry();
}
