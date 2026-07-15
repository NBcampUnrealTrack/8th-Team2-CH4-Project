// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/FTArenaGameState.h"
#include "FieryTaleLog.h"
#include "Net/UnrealNetwork.h"
#include "Character/FTPlayerState.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "Engine/DataTable.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

AFTArenaGameState::AFTArenaGameState()
{
	CurrentMatchState = EFTArenaMatchState::WaitingToStart;
}

void AFTArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTArenaGameState, CurrentMatchState);
	DOREPLIFETIME(AFTArenaGameState, BlueTeamKills);
	DOREPLIFETIME(AFTArenaGameState, RedTeamKills);
	
}

void AFTArenaGameState::SetArenaMatchState(EFTArenaMatchState NewState)
{
	if (HasAuthority() && CurrentMatchState != NewState)
	{
		CurrentMatchState = NewState;
		OnRep_CurrentMatchState();
	}
}

void AFTArenaGameState::AddTeamScore(EFTTeam ScoringTeam)
{
	if (HasAuthority())
	{
		if (ScoringTeam == EFTTeam::Blue) BlueTeamKills++;
		else if (ScoringTeam == EFTTeam::Red) RedTeamKills++;
	}
}

void AFTArenaGameState::OnRep_CurrentMatchState()
{
	UE_LOG(LogFTSession, Log, TEXT("[Arena] 전장 매치 상태 변경됨: %d"), (int32)CurrentMatchState);

	//	아레나가 시작(InProgress)된 직후, 이 머신에서 쓰일 캐릭터 관련 에셋을 미리 비동기 로딩한다.
	//	서버는 SetArenaMatchState가 이 함수를 직접 호출하고, 클라이언트는 RepNotify로 진입하므로 전 머신에서 실행된다.
	if (CurrentMatchState == EFTArenaMatchState::InProgress)
	{
		PreloadActiveCharacterAssets();
	}
}

void AFTArenaGameState::PreloadActiveCharacterAssets()
{
	//	InProgress는 한 번만 진입하지만, 방어적으로 중복 실행을 막는다.
	if (bPreloadStarted)
	{
		return;
	}

	//	데디케이티드 서버는 이펙트를 렌더링하지 않으므로 프리로드가 의미 없다.
	//	(리슨 서버 호스트는 직접 렌더링하므로 여기 걸리지 않고 정상 진행한다.)
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CharacterDataTable)
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Arena] CharacterDataTable 미지정 — 캐릭터 에셋 프리로드를 건너뜁니다."));
		return;
	}

	//	게임에 존재하는 캐릭터들의 PreloadAssets 경로를 중복 없이 모은다(같은 캐릭터 2명이어도 1회만 로드).
	TSet<FSoftObjectPath> PathsToLoad;
	for (APlayerState* PSBase : PlayerArray)
	{
		const AFTPlayerState* PS = Cast<AFTPlayerState>(PSBase);
		if (!PS)
		{
			continue;
		}

		//	행 이름 = EFTCharacterType 값 이름 (스폰 로직과 동일 규약). 타입이 아직 복제 전이면 None → 행 없음 → 스킵.
		const FName RowName(StaticEnum<EFTCharacterType>()->GetNameStringByValue(static_cast<int64>(PS->GetSelectedCharacterType())));
		const FFTCharacterData* Data = CharacterDataTable->FindRow<FFTCharacterData>(RowName, TEXT("PreloadActiveCharacterAssets"));
		if (!Data)
		{
			continue;
		}

		for (const TSoftObjectPtr<UObject>& Asset : Data->PreloadAssets)
		{
			if (!Asset.IsNull())
			{
				PathsToLoad.Add(Asset.ToSoftObjectPath());
			}
		}
	}

	if (PathsToLoad.Num() == 0)
	{
		return;
	}

	bPreloadStarted = true;

	//	비동기 로딩 시작. 반환된 핸들을 멤버에 보관해야 로드된 에셋이 매치 동안 메모리에 유지된다.
	FStreamableManager& Streamable = UAssetManager::Get().GetStreamableManager();
	PreloadHandle = Streamable.RequestAsyncLoad(
		PathsToLoad.Array(),
		FStreamableDelegate::CreateUObject(this, &AFTArenaGameState::OnPreloadComplete));

	UE_LOG(LogFTSession, Log, TEXT("[Arena] 캐릭터 관련 에셋 %d개 비동기 프리로드 시작"), PathsToLoad.Num());
}

void AFTArenaGameState::OnPreloadComplete()
{
	UE_LOG(LogFTSession, Log, TEXT("[Arena] 캐릭터 관련 에셋 프리로드 완료"));
}
