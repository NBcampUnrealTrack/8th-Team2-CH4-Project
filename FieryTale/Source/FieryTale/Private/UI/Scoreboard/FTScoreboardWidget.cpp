// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Scoreboard/FTScoreboardWidget.h"
#include "UI/Scoreboard/FTScoreboardRowWidget.h"
#include "Components/VerticalBox.h"
#include "Character/FTPlayerState.h"
#include "Character/FTCharacterTypes.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"

void UFTScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	//	숨겨져 있으면(Collapsed) 아무 것도 하지 않는다 — TAB을 눌러 표시 중일 때만 갱신한다.
	if (!IsVisible())
	{
		return;
	}

	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	//	인원이 바뀌었으면(입장/퇴장) 행을 다시 만들고, 그대로면 점수만 갱신한다.
	if (GameState->PlayerArray.Num() != CachedPlayerCount)
	{
		RebuildRows();
	}
	else
	{
		RefreshScores();
	}
}

void UFTScoreboardWidget::RebuildRows()
{
	if (!Box_BlueTeam || !Box_RedTeam || !RowWidgetClass)
	{
		return;
	}

	Box_BlueTeam->ClearChildren();
	Box_RedTeam->ClearChildren();
	CachedPlayers.Reset();
	CachedRows.Reset();

	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		CachedPlayerCount = -1;
		return;
	}

	for (APlayerState* PSBase : GameState->PlayerArray)
	{
		AFTPlayerState* PS = Cast<AFTPlayerState>(PSBase);
		if (!PS)
		{
			continue;
		}

		UFTScoreboardRowWidget* Row = CreateWidget<UFTScoreboardRowWidget>(this, RowWidgetClass);
		if (!Row)
		{
			continue;
		}

		//	캐릭터 데이터(초상화 + 표시 이름)를 조회한다. 없으면 이름은 비우고 포트레이트는 BP 기본값 유지.
		FText CharacterName = FText::GetEmpty();
		UTexture2D* Portrait = nullptr;
		if (const FFTCharacterData* Data = FindCharacterData(PS->GetSelectedCharacterType()))
		{
			CharacterName = Data->DisplayName;
			Portrait = Data->PortraitIcon.LoadSynchronous();
		}

		Row->SetupRow(FText::FromString(PS->GetPlayerName()), CharacterName, Portrait, PS->GetKills(), PS->GetDeaths());

		//	팀에 따라 두 박스 중 하나에 넣는다.
		UVerticalBox* TargetBox = (PS->GetTeam() == EFTTeam::Blue) ? Box_BlueTeam : Box_RedTeam;
		TargetBox->AddChildToVerticalBox(Row);

		CachedPlayers.Add(PS);
		CachedRows.Add(Row);
	}

	CachedPlayerCount = GameState->PlayerArray.Num();
}

void UFTScoreboardWidget::RefreshScores()
{
	for (int32 i = 0; i < CachedPlayers.Num(); ++i)
	{
		AFTPlayerState* PS = CachedPlayers[i].Get();
		UFTScoreboardRowWidget* Row = CachedRows.IsValidIndex(i) ? CachedRows[i].Get() : nullptr;

		//	퇴장 등으로 PlayerState가 무효화됐을 수 있으니 방어적으로 확인한다.
		if (IsValid(PS) && Row)
		{
			Row->UpdateScore(PS->GetKills(), PS->GetDeaths());
		}
	}
}

const FFTCharacterData* UFTScoreboardWidget::FindCharacterData(EFTCharacterType Type) const
{
	if (!CharacterDataTable)
	{
		return nullptr;
	}

	//	행 이름 = EFTCharacterType 값 이름 (SpawnCharacter와 동일 규약).
	const FName RowName(StaticEnum<EFTCharacterType>()->GetNameStringByValue(static_cast<int64>(Type)));
	return CharacterDataTable->FindRow<FFTCharacterData>(RowName, TEXT("Scoreboard::FindCharacterData"));
}
