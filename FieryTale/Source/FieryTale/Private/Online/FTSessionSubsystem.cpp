// Fill out your copyright notice in the Description page of Project Settings.


#include "Online/FTSessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// 세션 설정에 저장하는 커스텀 키. 검색 시 FieryTale 방만 골라내고, 방 이름/비밀번호를 주고받는다.
const FName UFTSessionSubsystem::Key_MatchType = FName(TEXT("FT_MATCHTYPE"));
const FName UFTSessionSubsystem::Key_RoomName = FName(TEXT("FT_ROOMNAME"));
const FName UFTSessionSubsystem::Key_Password = FName(TEXT("FT_PASSWORD"));

namespace
{
	// 이 게임의 세션을 식별하는 값. 다른 OnlineSubsystem Null 게임과 섞이지 않게 한다.
	const FString FieryTaleMatchType = TEXT("FieryTaleMatch");
}

UFTSessionSubsystem::UFTSessionSubsystem()
{
}

void UFTSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}

	// IOnlineSession 콜백을 멤버 함수로 한 번만 바인딩해 둔다. (실제 등록은 각 요청 시점에 핸들로 추가)
	CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleCreateSessionComplete);
	FindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleFindSessionsComplete);
	JoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleJoinSessionComplete);
	DestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleDestroySessionComplete);
	StartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleStartSessionComplete);
}

void UFTSessionSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFTSessionSubsystem::HostSession(int32 MaxPlayers, const FString& RoomName, const FString& Password, bool bUseLAN)
{
	if (!SessionInterface.IsValid())
	{
		OnCreateSessionCompleteEvent.Broadcast(false);
		return;
	}

	// 이미 세션이 살아 있으면 먼저 정리한 뒤, 파괴 완료 콜백에서 새로 만든다.
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bCreateSessionOnDestroyComplete = true;
		PendingMaxPlayers = MaxPlayers;
		PendingRoomName = RoomName;
		PendingPassword = Password;
		bPendingUseLAN = bUseLAN;
		LeaveSession();
		return;
	}

	CreateSessionInternal(MaxPlayers, RoomName, Password, bUseLAN);
}

void UFTSessionSubsystem::CreateSessionInternal(int32 MaxPlayers, const FString& RoomName, const FString& Password, bool bUseLAN)
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || LocalPlayer == nullptr)
	{
		OnCreateSessionCompleteEvent.Broadcast(false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bUseLAN;
	Settings.bIsDedicated = false;
	Settings.NumPublicConnections = FMath::Max(1, MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUsesPresence = !bUseLAN;
	Settings.bUseLobbiesIfAvailable = !bUseLAN;
	Settings.bAllowInvites = true;

	Settings.Set(Key_MatchType, FieryTaleMatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(Key_RoomName, RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(Key_Password, Password, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	OnCreateSessionCompleteEvent.Broadcast(bWasSuccessful);

	// 성공하면 호스트는 Listen 서버로 대기방 맵으로 이동한다.
	if (bWasSuccessful && bTravelToLobbyOnHost)
	{
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(LobbyMapPath + TEXT("?listen"));
		}
	}
}

void UFTSessionSubsystem::FindSessions(int32 MaxResults, bool bUseLAN)
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || LocalPlayer == nullptr)
	{
		OnFindSessionsCompleteEvent.Broadcast(TArray<FFTSessionInfo>(), false);
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxResults);
	SessionSearch->bIsLanQuery = bUseLAN;
	// FieryTale 방만 검색한다.
	SessionSearch->QuerySettings.Set(Key_MatchType, FieryTaleMatchType, EOnlineComparisonOp::Equals);
	if (!bUseLAN)
	{
		// UE 5.8에는 SEARCH_PRESENCE가 없다. 프레즌스 검색은 SEARCH_LOBBIES로 대체됨.
		// (호스트가 CreateSession 시 bUseLobbiesIfAvailable=true 로 만든 로비를 검색)
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnFindSessionsCompleteEvent.Broadcast(TArray<FFTSessionInfo>(), false);
	}
}

void UFTSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	TArray<FFTSessionInfo> Results;

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[Index];

			FFTSessionInfo Info;
			Info.SearchIndex = Index;

			FString RoomName;
			Result.Session.SessionSettings.Get(Key_RoomName, RoomName);
			Info.RoomName = RoomName.IsEmpty() ? Result.Session.OwningUserName : RoomName;

			Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
			Info.PingMs = Result.PingInMs;

			FString Password;
			Result.Session.SessionSettings.Get(Key_Password, Password);
			Info.bHasPassword = !Password.IsEmpty();

			Info.OwningPlayerName = Result.Session.OwningUserName;

			Results.Add(Info);
		}
	}

	OnFindSessionsCompleteEvent.Broadcast(Results, bWasSuccessful && Results.Num() > 0);
}

void UFTSessionSubsystem::JoinSessionByIndex(int32 SearchIndex, const FString& Password)
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || !SessionSearch.IsValid() || LocalPlayer == nullptr ||
		!SessionSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		OnJoinSessionCompleteEvent.Broadcast(false);
		return;
	}

	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchIndex];

	// 비밀번호 방이면 입력값이 일치해야 한다. (프로토타입 수준의 클라이언트 검증)
	FString StoredPassword;
	Result.Session.SessionSettings.Get(Key_Password, StoredPassword);
	if (!StoredPassword.IsEmpty() && StoredPassword != Password)
	{
		OnJoinSessionCompleteEvent.Broadcast(false);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinSessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);

	if (bSuccess)
	{
		// 호스트가 열어 둔 대기방으로 ClientTravel 한다.
		FString ConnectString;
		APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		if (PlayerController != nullptr && SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
		{
			PlayerController->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
		}
	}

	OnJoinSessionCompleteEvent.Broadcast(bSuccess);
}

void UFTSessionSubsystem::LeaveSession()
{
	if (!SessionInterface.IsValid())
	{
		OnDestroySessionCompleteEvent.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		OnDestroySessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	OnDestroySessionCompleteEvent.Broadcast(bWasSuccessful);

	// HostSession 이 "기존 세션 정리 후 재생성"을 요청한 경우 여기서 새로 만든다.
	if (bWasSuccessful && bCreateSessionOnDestroyComplete)
	{
		bCreateSessionOnDestroyComplete = false;
		CreateSessionInternal(PendingMaxPlayers, PendingRoomName, PendingPassword, bPendingUseLAN);
	}
}

void UFTSessionSubsystem::StartMatchSession()
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
}

void UFTSessionSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
}
