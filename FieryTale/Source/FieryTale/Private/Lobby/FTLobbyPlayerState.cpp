// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerState.h"

#include "FieryTaleLog.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h" // TActorIterator를 사용하기 위해 추가
#include "Core/Game/FTGameMode.h"
#include "Character/FTPlayerState.h" // 매치 정본 PlayerState (ASC 보유)
#include "Lobby/FTCharacterDisplayStand.h" //진열대 헤더 추가

AFTLobbyPlayerState::AFTLobbyPlayerState()
{
	SelectedCharacter = EFTCharacterType::None;
	bReplicates = true;
}

void AFTLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTLobbyPlayerState, bIsReady);
	
	DOREPLIFETIME(AFTLobbyPlayerState, PlayerIndex);
	DOREPLIFETIME(AFTLobbyPlayerState, SelectedCharacter);
}

void AFTLobbyPlayerState::SetReady(bool bInReady)
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 3) 서버 SetReady: %s, %s → %s"),
		*GetPlayerName(), bIsReady ? TEXT("Ready") : TEXT("NotReady"),
		bInReady ? TEXT("Ready") : TEXT("NotReady"));

	if (bIsReady != bInReady)
	{
		bIsReady = bInReady;
		// Listen 서버(호스트)의 로컬 UI는 OnRep 이 호출되지 않으므로 여기서 직접 알린다.
		OnReadyStateChanged.Broadcast();
	}
}

void AFTLobbyPlayerState::OnRep_IsReady()
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 4) 복제 수신(클라) OnRep_IsReady: %s → %s"),
		*GetPlayerName(), bIsReady ? TEXT("Ready") : TEXT("NotReady"));
	OnReadyStateChanged.Broadcast();
}

void AFTLobbyPlayerState::SetCharacterType(EFTCharacterType InCharacterType)
{
	if (HasAuthority())
	{
		if (SelectedCharacter != InCharacterType)
		{
			SelectedCharacter = InCharacterType;
			
			// 🌟 [추가]: 서버에서 진짜 값이 몇 번으로 바뀌었는지 찍어봅니다.
			UE_LOG(LogFTSession, Log, TEXT("[CharacterDebug] 서버 권한으로 %s의 캐릭터를 %d번으로 변경 완료!"), 
				*GetPlayerName(), (int32)SelectedCharacter);
			
			// 🌟 서버는 스스로 호출, 클라이언트는 OnRep이 자동 호출됨
			OnRep_SelectedCharacter(); 
		}
	}
}

void AFTLobbyPlayerState::CopyProperties(APlayerState* PlayerState)
{
	if (PlayerState)
	{
		PlayerState->SetPlayerName(GetPlayerName());
	}
	
	Super::CopyProperties(PlayerState);

	// 심리스 트래블 시 로비에서 고른 캐릭터를 매치 PlayerState로 넘긴다.
	if (AFTPlayerState* ArenaPS = Cast<AFTPlayerState>(PlayerState))
	{
		ArenaPS->SetSelectedCharacterType(this->SelectedCharacter);

		UE_LOG(LogFTSession, Log, TEXT("[SeamlessTravel] %s 님의 데이터 복사 성공! 캐릭터: %d"),
			*GetPlayerName(), (int32)this->SelectedCharacter);
	}
}

void AFTLobbyPlayerState::OnRep_SelectedCharacter()
{
	UE_LOG(LogFTSession, Log, TEXT("[Character] OnRep_SelectedCharacter 호출됨! OwnerName: %s"), GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"));

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AFTCharacterDisplayStand> It(World); It; ++It)
		{
			AFTCharacterDisplayStand* Stand = *It;
			// 로그로 StandIndex 비교 과정 확인
			UE_LOG(LogFTSession, Log, TEXT("[Character] Stand 검사 중: StandIndex=%d, 내 PlayerIndex=%d"), Stand->StandIndex, PlayerIndex);
            
			if (Stand && Stand->StandIndex == PlayerIndex)
			{
				Stand->UpdateCharacter(SelectedCharacter);
				return; // 찾았으면 종료
			}
		}
	}
}
