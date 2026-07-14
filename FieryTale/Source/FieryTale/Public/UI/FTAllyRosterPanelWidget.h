// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTAllyRosterPanelWidget.generated.h"

class UPanelWidget;
class UFTAllyRosterRowWidget;

// 아군 로스터 패널 — Container(VerticalBox)에 팀원 순서(PlayerIndex)대로 Row를 쌓는다.
UCLASS()
class FIERYTALE_API UFTAllyRosterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Container;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI")
	TSubclassOf<UFTAllyRosterRowWidget> RowWidgetClass;

	// 팀원 줄 사이 공백
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI")
	float RowSpacing = 8.0f;

private:
	void PopulateRoster();

	// PlayerArray가 아직 다 안 채워진 상태로 NativeConstruct가 먼저 실행될 수 있어서,
	// FTLobbyWidget과 동일하게 인원수 변화를 주기적으로 감지해 재구성한다.
	void CheckRosterCount();

	int32 CachedPlayerCount = -1;
	FTimerHandle RosterCheckTimerHandle;
};
