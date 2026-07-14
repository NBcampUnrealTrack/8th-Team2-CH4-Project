// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FTStructureStatusPanelWidget.h"
#include "UI/FTStructureStatusRowWidget.h"
#include "Level/FTBattleSubsystem.h"
#include "Level/FTNexus.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

void UFTStructureStatusPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PopulateTeamTowers(EFTTurretTeam::BlueTeam, BlueTowerContainer);
	PopulateTeamTowers(EFTTurretTeam::RedTeam, RedTowerContainer);
	PopulateNexusRow(EFTTurretTeam::BlueTeam, BlueNexusRow);
	PopulateNexusRow(EFTTurretTeam::RedTeam, RedNexusRow);
}

void UFTStructureStatusPanelWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UFTStructureStatusPanelWidget::PopulateTeamTowers(EFTTurretTeam Team, UPanelWidget* Container)
{
	if (!Container || !TowerRowWidgetClass)
	{
		return;
	}

	UFTBattleSubsystem* Battle = GetWorld() ? GetWorld()->GetSubsystem<UFTBattleSubsystem>() : nullptr;
	if (!Battle)
	{
		return;
	}

	for (AFTTurret* Turret : Battle->GetTurrets(Team))
	{
		if (!Turret)
		{
			continue;
		}

		UFTStructureStatusRowWidget* Row = CreateWidget<UFTStructureStatusRowWidget>(this, TowerRowWidgetClass);
		if (!Row)
		{
			continue;
		}

		Row->InitializeWithStructure(Turret);

		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Container->AddChild(Row)))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, TowerRowSpacing));
		}
	}
}

void UFTStructureStatusPanelWidget::PopulateNexusRow(EFTTurretTeam Team, UFTStructureStatusRowWidget* Row)
{
	if (!Row)
	{
		return;
	}

	UFTBattleSubsystem* Battle = GetWorld() ? GetWorld()->GetSubsystem<UFTBattleSubsystem>() : nullptr;
	AFTNexus* Nexus = Battle ? Battle->GetNexus(Team) : nullptr;
	if (!Nexus)
	{
		return;
	}

	Row->InitializeWithStructure(Nexus);
}
