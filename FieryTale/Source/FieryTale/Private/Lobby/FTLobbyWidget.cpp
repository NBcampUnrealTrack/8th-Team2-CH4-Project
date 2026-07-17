// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "GameFramework/GameStateBase.h"

#include "Character/FTPlayerController.h"
#include "Character/FTCharacterTypes.h"
#include "Lobby/FTLobbyPlayerState.h"
#include "Character/FTCharacterTypes.h"

#include "Engine/DataTable.h"
#include "Components/PanelWidget.h" // 컨테이너 조작용
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"

#include "Algo/Sort.h"
#include "Lobby/FTCharacterSelectButton.h"

void UFTLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// 버튼 이벤트 바인딩
	if (Btn_Ready)
	{
		Btn_Ready->OnClicked.AddDynamic(this, &UFTLobbyWidget::OnReadyClicked);
	}
	if (Btn_StartGame)
	{
		Btn_StartGame->OnClicked.AddDynamic(this, &UFTLobbyWidget::OnStartGameClicked);
	}
	if (Btn_ReturnToMain)
	{
		Btn_ReturnToMain->OnClicked.AddDynamic(this, &UFTLobbyWidget::OnReturnToMainClicked);
	}
}

void UFTLobbyWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RosterCheckTimerHandle);
	}
	Super::NativeDestruct();
}

void UFTLobbyWidget::InitWidget(AFTPlayerController* InPC, AFTLobbyPlayerState* InPS)
{
	LobbyPC = InPC;
	LobbyPS = InPS;
	
	if (LobbyPS)
	{
		// 안전하게 바인딩
		LobbyPS->OnReadyStateChanged.RemoveDynamic(this, &UFTLobbyWidget::OnReadyStateChanged); // 중복 방지
		LobbyPS->OnReadyStateChanged.AddDynamic(this, &UFTLobbyWidget::OnReadyStateChanged);
		
		// 현재 상태 반영
		OnReadyStateChanged();
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RosterCheckTimerHandle, this, &UFTLobbyWidget::CheckRosterCount, 0.5f, true);
	}
	
	// 🌟 [추가]: 데이터 테이블 기반 동적 버튼 생성 로직
	if (CharacterDataTable && CharacterButtonContainer && CharacterSelectButtonClass)
	{
		CharacterButtonContainer->ClearChildren();

		// 데이터 테이블의 모든 행 가져오기
		static const FString ContextString(TEXT("Lobby Character Initialization"));
		TArray<FFTCharacterData*> AllCharacters;
		CharacterDataTable->GetAllRows<FFTCharacterData>(ContextString, AllCharacters);

		// 영웅 개수만큼 버튼을 만들고 데이터를 주입
		for (FFTCharacterData* CharData : AllCharacters)
		{
			if (CharData && CharData->CharacterType != EFTCharacterType::None)
			{
				UFTCharacterSelectButton* NewButtonWidget = CreateWidget<UFTCharacterSelectButton>(this, CharacterSelectButtonClass);
                
				if (NewButtonWidget)
				{
					NewButtonWidget->InitializeButton(CharData, LobbyPC);

					CharacterButtonContainer->AddChild(NewButtonWidget);
				}
			}
		}
	}
	
	RefreshTeamRoster();
}

void UFTLobbyWidget::CheckRosterCount()
{
	if (AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		// 이전에 기억하던 인원수와 현재 GameState의 인원수가 다르면 누군가 들어오거나 나간 것!
		if (GameState->PlayerArray.Num() != CachedPlayerCount)
		{
			CachedPlayerCount = GameState->PlayerArray.Num();
			RefreshTeamRoster();
		}
	}
}

void UFTLobbyWidget::RefreshTeamRoster()
{
	if (!TeamRosterBox || !PlayerSlotWidgetClass) return;

	TeamRosterBox->ClearChildren();

	AGameStateBase* GameState = GetWorld()->GetGameState();
	TArray<APlayerState*> SortedArray;
	
	// 2. 현재 접속 중인 플레이어 배열을 가져와 정렬합니다.
	if (GameState)
	{
		SortedArray = GameState->PlayerArray;
		Algo::Sort(SortedArray, [](APlayerState* A, APlayerState* B) {
			return A->GetPlayerId() < B->GetPlayerId();
		});
	}

	// 무조건 최대 인원수(MaxPlayerCount)만큼 슬롯을 생성합니다.[cite: 9]
	for (int32 i = 0; i < MaxPlayerCount; ++i)
	{
		UFTLobbyPlayerSlot* PlayerSlot = CreateWidget<UFTLobbyPlayerSlot>(this, PlayerSlotWidgetClass);
		if (!PlayerSlot) continue;

		bool bSlotFilled = false;

		// 현재 인덱스(i)에 접속한 유저가 존재한다면
		if (SortedArray.IsValidIndex(i))
		{
			AFTLobbyPlayerState* TargetPS = Cast<AFTLobbyPlayerState>(SortedArray[i]);
			if (TargetPS)
			{
				TargetPS->OnCharacterStateChanged.RemoveDynamic(this, &UFTLobbyWidget::RefreshTeamRoster);
				TargetPS->OnCharacterStateChanged.AddDynamic(this, &UFTLobbyWidget::RefreshTeamRoster);
				
				TargetPS->OnReadyStateChanged.RemoveDynamic(this, &UFTLobbyWidget::RefreshTeamRoster);
				TargetPS->OnReadyStateChanged.AddDynamic(this, &UFTLobbyWidget::RefreshTeamRoster);
				
				PlayerSlot->UpdateSlotData(TargetPS->GetPlayerName(), TargetPS->GetCharacterType(), TargetPS->IsReady());
				bSlotFilled = true;
			}
		}

		// 접속한 유저가 없거나 빈자리로 판별된 인덱스는 완벽하게 빈 데이터 주입
		if (!bSlotFilled)
		{
			PlayerSlot->UpdateSlotData(TEXT(""), EFTCharacterType::None); 
		}

		// 패널에 추가합니다.[cite: 9]
		TeamRosterBox->AddChild(PlayerSlot);
	}
}

void UFTLobbyWidget::OnReadyClicked()
{
	if (LobbyPC && !LobbyPS)
	{
		LobbyPS = LobbyPC->GetPlayerState<AFTLobbyPlayerState>();
		if (LobbyPS)
		{
			LobbyPS->OnReadyStateChanged.AddDynamic(this, &UFTLobbyWidget::OnReadyStateChanged);
		}
	}

	if (LobbyPC && LobbyPS)
	{
		bool bNewReadyState = !LobbyPS->IsReady();
		LobbyPC->RequestSetReady(bNewReadyState);
	}
}

void UFTLobbyWidget::OnStartGameClicked()
{
	if (LobbyPC)
	{
		// 강제 게임 시작 요청
		LobbyPC->RequestStartMatch();
	}
}

void UFTLobbyWidget::OnReadyStateChanged()
{
	// bIsReady 값이 리플리케이트(OnRep) 되거나 로컬에서 변경될 때마다 호출됨
	if (LobbyPS && Text_ReadyStatus)
	{
		if (LobbyPS->IsReady())
		{
			Text_ReadyStatus->SetText(FText::FromString(TEXT("상태: 준비 완료!")));
			Text_ReadyStatus->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
		}
		else
		{
			Text_ReadyStatus->SetText(FText::FromString(TEXT("상태: 대기 중...")));
			Text_ReadyStatus->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

void UFTLobbyWidget::OnReturnToMainClicked()
{
	if (LobbyPC)
	{
		// 버튼이 여러 번 눌리는 것을 방지
		Btn_ReturnToMain->SetIsEnabled(false); 
		LobbyPC->LeaveLobby();
	}
}


