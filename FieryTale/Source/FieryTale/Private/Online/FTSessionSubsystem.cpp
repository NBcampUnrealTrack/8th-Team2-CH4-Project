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
#include "FieryTaleLog.h"

// LogFTSession 카테고리는 FieryTaleLog.h 에서 선언하고 여기서 정의한다. (세션+로비 네트워크 공용)
DEFINE_LOG_CATEGORY(LogFTSession);

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

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}

	UE_LOG(LogFTSession, Log, TEXT("[Init] 서브시스템 초기화: OSS=%s, SessionInterface=%s"),
		Subsystem ? *Subsystem->GetSubsystemName().ToString() : TEXT("NULL"),
		SessionInterface.IsValid() ? TEXT("OK") : TEXT("NULL"));

	// IOnlineSession 콜백을 멤버 함수로 한 번만 바인딩해 둔다. (실제 등록은 각 요청 시점에 핸들로 추가)
	CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleCreateSessionComplete);
	FindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleFindSessionsComplete);
	JoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleJoinSessionComplete);
	DestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleDestroySessionComplete);
	StartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleStartSessionComplete);
}

void UFTSessionSubsystem::Deinitialize()
{
	// PIE 종료/게임 종료 시 세션을 정리한다. OnlineSubsystem(Null)은 에디터 프로세스 단위 싱글톤이라
	// 여기서 파괴하지 않으면 GameSession 이 남아 다음 실행의 입장(JoinSession)을 막는다.
	// (teardown 중이라 콜백 없이 직접 파괴한다.)
	const bool bHadSession = SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
	UE_LOG(LogFTSession, Log, TEXT("[Deinit] 서브시스템 종료: 잔류 GameSession=%s"),
		bHadSession ? TEXT("있음 → 파괴") : TEXT("없음"));
	if (bHadSession)
	{
		SessionInterface->DestroySession(NAME_GameSession);
	}

	Super::Deinitialize();
}

void UFTSessionSubsystem::HostSession(int32 MaxPlayers, const FString& RoomName, const FString& Password, bool bUseLAN)
{
	UE_LOG(LogFTSession, Log, TEXT("[Host] 1) 요청: MaxPlayers=%d, RoomName=%s, 비번=%s, LAN=%s"),
		MaxPlayers, *RoomName, Password.IsEmpty() ? TEXT("없음") : TEXT("있음"), bUseLAN ? TEXT("true") : TEXT("false"));

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogFTSession, Error, TEXT("[Host] 1-실패) SessionInterface 무효 (OSS 미초기화)"));
		OnCreateSessionCompleteEvent.Broadcast(false);
		return;
	}

	// 이미 세션이 살아 있으면 먼저 정리한 뒤, 파괴 완료 콜백에서 새로 만든다.
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Host] 2) 기존 GameSession 존재 → 파괴 후 재생성 예약"));
		bCreateSessionOnDestroyComplete = true;
		PendingMaxPlayers = MaxPlayers;
		PendingRoomName = RoomName;
		PendingPassword = Password;
		bPendingUseLAN = bUseLAN;
		LeaveSession();
		return;
	}

	UE_LOG(LogFTSession, Log, TEXT("[Host] 2) 기존 세션 없음 → 바로 생성"));
	CreateSessionInternal(MaxPlayers, RoomName, Password, bUseLAN);
}

void UFTSessionSubsystem::CreateSessionInternal(int32 MaxPlayers, const FString& RoomName, const FString& Password, bool bUseLAN)
{
	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || LocalPlayer == nullptr)
	{
		UE_LOG(LogFTSession, Error, TEXT("[Create] 사전 검증 실패 (SessionInterface=%s, LocalPlayer=%s)"),
			SessionInterface.IsValid() ? TEXT("OK") : TEXT("NULL"), LocalPlayer ? TEXT("OK") : TEXT("NULL"));
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

	UE_LOG(LogFTSession, Log, TEXT("[Create] 3) CreateSession 호출: 정원=%d, LAN=%s, UseLobbies=%s - 콜백 대기"),
		Settings.NumPublicConnections, bUseLAN ? TEXT("true") : TEXT("false"),
		Settings.bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"));
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Settings))
	{
		UE_LOG(LogFTSession, Error, TEXT("[Create] 3-실패) CreateSession 이 즉시 false 반환"));
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

	UE_LOG(LogFTSession, Log, TEXT("[Create] 4) 콜백 수신: SessionName=%s, 성공=%s"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));

	OnCreateSessionCompleteEvent.Broadcast(bWasSuccessful);

	// 성공하면 호스트는 Listen 서버로 대기방 맵으로 이동한다.
	if (bWasSuccessful && bTravelToLobbyOnHost)
	{
		if (UWorld* World = GetWorld())
		{
			const FString TravelUrl = LobbyMapPath + TEXT("?listen");
			UE_LOG(LogFTSession, Log, TEXT("[Create] 5) 호스트 ServerTravel → %s"), *TravelUrl);
			World->ServerTravel(TravelUrl);
		}
		else
		{
			UE_LOG(LogFTSession, Error, TEXT("[Create] 5-실패) World 가 없어 ServerTravel 불가"));
		}
	}
}

void UFTSessionSubsystem::FindSessions(int32 MaxResults, bool bUseLAN)
{
	UE_LOG(LogFTSession, Log, TEXT("[Find] 1) 요청: MaxResults=%d, LAN=%s"),
		MaxResults, bUseLAN ? TEXT("true") : TEXT("false"));

	const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	if (!SessionInterface.IsValid() || LocalPlayer == nullptr)
	{
		UE_LOG(LogFTSession, Error, TEXT("[Find] 1-실패) 사전 검증 실패 (SessionInterface=%s, LocalPlayer=%s)"),
			SessionInterface.IsValid() ? TEXT("OK") : TEXT("NULL"), LocalPlayer ? TEXT("OK") : TEXT("NULL"));
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

	UE_LOG(LogFTSession, Log, TEXT("[Find] 2) FindSessions 호출 - 콜백 대기"));
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogFTSession, Error, TEXT("[Find] 2-실패) FindSessions 가 즉시 false 반환"));
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

	const int32 RawCount = (bWasSuccessful && SessionSearch.IsValid()) ? SessionSearch->SearchResults.Num() : 0;
	UE_LOG(LogFTSession, Log, TEXT("[Find] 3) 콜백 수신: 성공=%s, 원시 결과 %d개"),
		bWasSuccessful ? TEXT("true") : TEXT("false"), RawCount);

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

			UE_LOG(LogFTSession, Log, TEXT("[Find]    - [%d] %s (%d/%d) 비번=%s 핑=%dms"),
				Info.SearchIndex, *Info.RoomName, Info.CurrentPlayers, Info.MaxPlayers,
				Info.bHasPassword ? TEXT("Y") : TEXT("N"), Info.PingMs);
			Results.Add(Info);
		}
	}

	UE_LOG(LogFTSession, Log, TEXT("[Find] 4) 결과 브로드캐스트: 표시 %d개"), Results.Num());
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

	// 이전 PIE 실행 등에서 남은 GameSession 이 있으면 JoinSession 이 "already exists, can't join twice" 로
	// 실패한다. 먼저 파괴하고, 파괴 완료 콜백(HandleDestroySessionComplete)에서 다시 입장한다.
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Join] 4-1) 기존 GameSession 잔류 발견 → 먼저 파괴 후 재입장"));
		bJoinSessionOnDestroyComplete = true;
		PendingJoinIndex = SearchIndex;
		PendingJoinPassword = Password;
		LeaveSession();
		return;
	}

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
	const bool bHasSession = SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
	UE_LOG(LogFTSession, Log, TEXT("[Leave] 1) 요청: SessionInterface=%s, 현재 GameSession=%s"),
		SessionInterface.IsValid() ? TEXT("OK") : TEXT("NULL"), bHasSession ? TEXT("있음") : TEXT("없음"));

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogFTSession, Error, TEXT("[Leave] 1-실패) SessionInterface 무효"));
		OnDestroySessionCompleteEvent.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	UE_LOG(LogFTSession, Log, TEXT("[Leave] 2) DestroySession 호출 - 콜백 대기"));
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		UE_LOG(LogFTSession, Error, TEXT("[Leave] 2-실패) DestroySession 이 즉시 false 반환"));
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		OnDestroySessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogFTSession, Log, TEXT("[Leave] 3) 콜백 수신: SessionName=%s, 성공=%s (재생성예약=%s, 재입장예약=%s)"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"),
		bCreateSessionOnDestroyComplete ? TEXT("Y") : TEXT("N"),
		bJoinSessionOnDestroyComplete ? TEXT("Y") : TEXT("N"));

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	OnDestroySessionCompleteEvent.Broadcast(bWasSuccessful);

	// HostSession 이 "기존 세션 정리 후 재생성"을 요청한 경우 여기서 새로 만든다.
	if (bWasSuccessful && bCreateSessionOnDestroyComplete)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Leave] 4) 파괴 완료 → 보류된 방 생성 진행"));
		bCreateSessionOnDestroyComplete = false;
		CreateSessionInternal(PendingMaxPlayers, PendingRoomName, PendingPassword, bPendingUseLAN);
	}
	// JoinSessionByIndex 가 "기존 세션 정리 후 재입장"을 요청한 경우 여기서 다시 입장한다.
	// (파괴가 끝나 GameSession 이 사라졌으므로 이번엔 JoinSession 이 성공한다.)
	else if (bWasSuccessful && bJoinSessionOnDestroyComplete)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Leave] 4) 파괴 완료 → 보류된 입장 재시도"));
		bJoinSessionOnDestroyComplete = false;
		JoinSessionByIndex(PendingJoinIndex, PendingJoinPassword);
	}
}

void UFTSessionSubsystem::StartMatchSession()
{
	UE_LOG(LogFTSession, Log, TEXT("[Start] 1) StartSession 요청"));
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogFTSession, Error, TEXT("[Start] 1-실패) SessionInterface 무효"));
		return;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		UE_LOG(LogFTSession, Error, TEXT("[Start] 1-실패) StartSession 이 즉시 false 반환"));
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
}

void UFTSessionSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogFTSession, Log, TEXT("[Start] 2) 콜백 수신: SessionName=%s, 성공=%s"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
}
