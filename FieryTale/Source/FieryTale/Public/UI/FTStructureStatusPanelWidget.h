// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Level/FTTurret.h"
#include "GameplayTagContainer.h"
#include "FTStructureStatusPanelWidget.generated.h"

class UPanelWidget;
class UFTStructureStatusRowWidget;
class AFTTowerBase;
class AFTNexus;


UCLASS()
class FIERYTALE_API UFTStructureStatusPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> BlueTowerContainer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RedTowerContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UFTStructureStatusRowWidget> BlueNexusRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UFTStructureStatusRowWidget> RedNexusRow;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI")
	TSubclassOf<UFTStructureStatusRowWidget> TowerRowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI")
	float TowerRowSpacing = 6.0f;

private:
	void PopulateTeamTowers(EFTTurretTeam Team, UPanelWidget* Container);
	void PopulateNexusRow(EFTTurretTeam Team, UFTStructureStatusRowWidget* Row);
};
