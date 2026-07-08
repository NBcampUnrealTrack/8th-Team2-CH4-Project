// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerState.h"

#include "FieryTaleLog.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h" // TActorIterator를 사용하기 위해 추가
#include "Core/Game/FTGameMode.h"
#include "Character/FTPlayerState.h"
#include "Lobby/FTCharacterDisplayStand.h" //진열대 헤더 추가
#include "Log/FTLogSubsystem.h"

AFTLobbyPlayerState::AFTLobbyPlayerState()
{
	SelectedCharacterType = EFTCharacterType::None;
	bReplicates = true;
}

void AFTLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTLobbyPlayerState, bIsReady);
	
	DOREPLIFETIME(AFTLobbyPlayerState, PlayerIndex);
	DOREPLIFETIME(AFTLobbyPlayerState, SelectedCharacterType);
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
	/*
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 4) 복제 수신(클라) OnRep_IsReady: %s → %s"),
		*GetPlayerName(), bIsReady ? TEXT("Ready") : TEXT("NotReady"));
	OnReadyStateChanged.Broadcast();
	//*/
	FT_LOG(this, EFTLogLevel::Log, TEXT("[Ready] 4) 복제 수신(클라) OnRep_IsReady: %s → %s"),
		*GetPlayerName(), bIsReady ? TEXT("Ready") : TEXT("NotReady"));
}

void AFTLobbyPlayerState::SetCharacterType(EFTCharacterType InCharacterType)
{
	if (HasAuthority() && SelectedCharacterType != InCharacterType)
	{
		SelectedCharacterType = InCharacterType;
		
		// 🌟 방장 환경에서도 UI 갱신과 3D 단상 갱신이 정상적으로 처리되도록 강제 호출
		OnRep_SelectedCharacter(); 
	}
}

void AFTLobbyPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	
	if (PlayerState)
	{
		PlayerState->SetPlayerName(GetPlayerName());
	}
	
	if (AFTPlayerState* PS = Cast<AFTPlayerState>(PlayerState))
	{
		// 캐릭터 타입과 플레이어 인덱스를 넘겨주기		
		PS->SetSelectedCharacterType(this->SelectedCharacterType);
		PS->SetPlayerIndex(this->PlayerIndex);
		
		UE_LOG(LogFTSession, Log, TEXT("[SeamlessTravel] %s 님의 데이터 복사 성공! 캐릭터: %d, 인덱스: %d"), 
			*GetPlayerName(), (int32)PS->GetSelectedCharacterType(), PS->GetPlayerIndex());
	}
}

void AFTLobbyPlayerState::OnRep_SelectedCharacter()
{
	OnCharacterStateChanged.Broadcast();

	APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	if (LocalPC && LocalPC->PlayerState == this)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Character] 내 캐릭터가 변경됨! 로컬 스탠드 업데이트 시작"));

		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AFTCharacterDisplayStand> It(World); It; ++It)
			{
				if (AFTCharacterDisplayStand* Stand = *It)
				{
					Stand->UpdateCharacter(SelectedCharacterType);
					return; // 단상을 찾아서 갱신했으면 종료
				}
			}
		}
	}
}
