// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FTAllyRosterPanelWidget.h"
#include "UI/FTAllyRosterRowWidget.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerController.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UFTAllyRosterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PopulateRoster();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RosterCheckTimerHandle, this, &UFTAllyRosterPanelWidget::CheckRosterCount, 0.5f, true);
	}
}

void UFTAllyRosterPanelWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RosterCheckTimerHandle);
	}
	Super::NativeDestruct();
}

void UFTAllyRosterPanelWidget::CheckRosterCount()
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	// 다른 플레이어의 PlayerState가 아직 리플리케이트 중일 때 NativeConstruct가 먼저 실행되는
	// 경우가 있어서, 인원수가 바뀌면 재구성한다 (FTLobbyWidget과 동일한 패턴).
	if (GameState->PlayerArray.Num() != CachedPlayerCount)
	{
		CachedPlayerCount = GameState->PlayerArray.Num();
		PopulateRoster();
	}
}

void UFTAllyRosterPanelWidget::PopulateRoster()
{
	if (!Container || !RowWidgetClass)
	{
		return;
	}

	APlayerController* LocalPC = GetOwningPlayer();
	if (!LocalPC)
	{
		return;
	}

	AFTPlayerState* LocalPS = LocalPC->GetPlayerState<AFTPlayerState>();
	if (!LocalPS)
	{
		// 로컬 PlayerState가 아직 배정 전 — 배정되는 순간 재시도
		if (AFTPlayerController* LocalFTPC = Cast<AFTPlayerController>(LocalPC))
		{
			LocalFTPC->OnPlayerStateReady.RemoveAll(this);
			LocalFTPC->OnPlayerStateReady.AddUObject(this, &UFTAllyRosterPanelWidget::PopulateRoster);
		}
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	TArray<AFTPlayerState*> Teammates;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (AFTPlayerState* FTPS = Cast<AFTPlayerState>(PS))
		{
			if (FTPS->GetTeam() == LocalPS->GetTeam())
			{
				Teammates.Add(FTPS);
			}
		}
	}

	Teammates.Sort([](const AFTPlayerState& A, const AFTPlayerState& B)
	{
		return A.GetPlayerIndex() < B.GetPlayerIndex();
	});

	Container->ClearChildren();
	for (AFTPlayerState* Teammate : Teammates)
	{
		UFTAllyRosterRowWidget* Row = CreateWidget<UFTAllyRosterRowWidget>(this, RowWidgetClass);
		if (!Row)
		{
			continue;
		}

		Row->InitializeWithPlayerState(Teammate);

		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Container->AddChild(Row)))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, RowSpacing));
		}
	}
}
