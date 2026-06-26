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

// 세션 흐름(특히 JoinSessionByIndex)을 단계별로 추적하기 위한 로그 카테고리.
// 콘솔/Output Log 에서 "LogFTSession" 으로 필터링하면 입장 과정을 한눈에 볼 수 있다.
DEFINE_LOG_CATEGORY_STATIC(LogFTSession, Log, All);

// 세션 설정에 저장하는 커스텀 키. 검색 시 FieryTale 방만 골라내고, 방 이름/비밀번호를 주고받는다.
const FName UFTSessionSubsystem::Key_MatchType = FName(TEXT("FT_MATCHTYPE"));
const FName UFTSessionSubsystem::Key_RoomName = FName(TEXT("FT_ROOMNAME"));
const FName UFTSessionSubsystem::Key_Password = FName(TEXT("FT_PASSWORD"));

namespace
{
	// 이 게임의 세션을 식별하는 값. 다른 OnlineSubsystem Null 게임과 섞이지 않게 한다.
	const FString FieryTaleMatchType = TEXT("FieryTaleMatch");

	// EOnJoinSessionCompleteResult 열거형을 로그용 문자열로 변환한다.
	const TCHAR* LexJoinResult(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success:                 return TEXT("Success");
		case EOnJoinSessionCompleteResult::SessionIsFull:           return TEXT("SessionIsFull");
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:     return TEXT("SessionDoesNotExist");
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: return TEXT("CouldNotRetrieveAddress");
		case EOnJoinSessionCompleteResult::AlreadyInSession:        return TEXT("AlreadyInSession");
		case EOnJoinSessionCompleteResult::UnknownError:            return TEXT("UnknownError");
		default:                                                    return TEXT("Unhandled");
		}
	}
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
	UE_LOG(LogFTSession, Log, TEXT("[Join] 1) 요청 시작: SearchIndex=%d, Password=%s"),
		SearchIndex, Password.IsEmpty() ? TEXT("(없음)") : TEXT("(입력됨)"));

	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || !SessionSearch.IsValid() || LocalPlayer == nullptr ||
		!SessionSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		UE_LOG(LogFTSession, Warning,
			TEXT("[Join] 2) 사전 검증 실패 - 입장 불가 (SessionInterface=%s, SessionSearch=%s, LocalPlayer=%s, IndexValid=%s)"),
			SessionInterface.IsValid() ? TEXT("OK") : TEXT("NULL"),
			SessionSearch.IsValid() ? TEXT("OK") : TEXT("NULL"),
			LocalPlayer ? TEXT("OK") : TEXT("NULL"),
			(SessionSearch.IsValid() && SessionSearch->SearchResults.IsValidIndex(SearchIndex)) ? TEXT("OK") : TEXT("INVALID"));
		OnJoinSessionCompleteEvent.Broadcast(false);
		return;
	}
	UE_LOG(LogFTSession, Log, TEXT("[Join] 2) 사전 검증 통과 (검색 결과 %d개 중 %d번 선택)"),
		SessionSearch->SearchResults.Num(), SearchIndex);

	const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchIndex];

	FString TargetRoomName;
	Result.Session.SessionSettings.Get(Key_RoomName, TargetRoomName);
	UE_LOG(LogFTSession, Log, TEXT("[Join] 3) 대상 세션: RoomName=%s, Owner=%s, Ping=%dms"),
		TargetRoomName.IsEmpty() ? TEXT("(없음)") : *TargetRoomName,
		*Result.Session.OwningUserName,
		Result.PingInMs);

	// 비밀번호 방이면 입력값이 일치해야 한다. (프로토타입 수준의 클라이언트 검증)
	FString StoredPassword;
	Result.Session.SessionSettings.Get(Key_Password, StoredPassword);
	if (!StoredPassword.IsEmpty() && StoredPassword != Password)
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Join] 4) 비밀번호 불일치 - 입장 거부"));
		OnJoinSessionCompleteEvent.Broadcast(false);
		return;
	}
	UE_LOG(LogFTSession, Log, TEXT("[Join] 4) 비밀번호 검증 통과 (%s)"),
		StoredPassword.IsEmpty() ? TEXT("비번 없는 방") : TEXT("비번 일치"));

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	UE_LOG(LogFTSession, Log, TEXT("[Join] 5) JoinSession 호출 - 비동기 콜백(HandleJoinSessionComplete) 대기"));
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result))
	{
		UE_LOG(LogFTSession, Error, TEXT("[Join] 5-실패) JoinSession 이 즉시 false 반환 - 콜백 없이 종료"));
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinSessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogFTSession, Log, TEXT("[Join] 6) 콜백 수신: SessionName=%s, Result=%s"),
		*SessionName.ToString(), LexJoinResult(Result));

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
			UE_LOG(LogFTSession, Log, TEXT("[Join] 7) 접속 주소 확보: %s -> ClientTravel 실행"), *ConnectString);
			PlayerController->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
		}
		else
		{
			UE_LOG(LogFTSession, Error,
				TEXT("[Join] 7-실패) Join 은 성공했으나 ClientTravel 불가 (PlayerController=%s, ResolvedConnectString 확보 실패)"),
				PlayerController ? TEXT("OK") : TEXT("NULL"));
		}
	}

	UE_LOG(LogFTSession, Log, TEXT("[Join] 8) 결과 브로드캐스트: bSuccess=%s"), bSuccess ? TEXT("true") : TEXT("false"));
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
