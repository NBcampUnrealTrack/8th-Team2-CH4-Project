// Fill out your copyright notice in the Description page of Project Settings.

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
#include "Interfaces/OnlineIdentityInterface.h"
#include "Misc/CommandLine.h"

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
	// 기본 대기방 레벨. 에디터/BP에서 LobbyLevel 을 지정하면 그 값이 우선한다.
	LobbyLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/L_Lobby.L_Lobby")));
}

void UFTSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 🌟 수정 1: Initialize 시점에는 GetWorld()가 없을 수 있으므로 글로벌 캐싱을 제거합니다.
	UE_LOG(LogFTSession, Log, TEXT("[Init] 서브시스템 초기화 (SessionInterface는 각 요청 시 동적으로 가져옵니다)"));

	// IOnlineSession 콜백을 멤버 함수로 한 번만 바인딩해 둔다.
	CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleCreateSessionComplete);
	FindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleFindSessionsComplete);
	JoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleJoinSessionComplete);
	DestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleDestroySessionComplete);
	StartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleStartSessionComplete);
}

void UFTSessionSubsystem::Deinitialize()
{
	// 🌟 수정 2: 각 함수 시작 시 현재 PIE 창에 맞는 로컬 Subsystem과 SessionInterface를 갱신합니다.
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

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
	// 🌟 동적 갱신
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

	UE_LOG(LogFTSession, Log, TEXT("[Host] 1) 요청: MaxPlayers=%d, RoomName=%s, 비번=%s, LAN=%s"),
		MaxPlayers, *RoomName, Password.IsEmpty() ? TEXT("없음") : TEXT("있음"), bUseLAN ? TEXT("true") : TEXT("false"));

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogFTSession, Error, TEXT("[Host] 1-실패) SessionInterface 무효 (OSS 미초기화)"));
		OnCreateSessionCompleteEvent.Broadcast(false);
		return;
	}

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
	// 🌟 로컬 창구 통일 및 동적 갱신
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (Subsystem) SessionInterface = Subsystem->GetSessionInterface();
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr UserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;

	if (!SessionInterface.IsValid() || !UserId.IsValid())
	{
		UE_LOG(LogFTSession, Error, TEXT("[Create] 검증 실패 (유효한 UserId가 없거나 SessionInterface 무효)"));
		OnCreateSessionCompleteEvent.Broadcast(false);
		return;
	}

	FOnlineSessionSettings Settings;
	
	Settings.bIsLANMatch = false; 
	Settings.bIsDedicated = false; 
	Settings.bUsesPresence = true; 
	Settings.bUseLobbiesIfAvailable = true; 
	Settings.bAllowJoinViaPresence = true; 
	Settings.bAllowJoinInProgress = true;
	Settings.bShouldAdvertise = true;
	Settings.bAllowInvites = true;
	Settings.NumPublicConnections = FMath::Max(1, MaxPlayers);
	Settings.NumPrivateConnections = 0;

	Settings.Set(Key_MatchType, FieryTaleMatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(Key_RoomName, RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(Key_Password, Password, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	UE_LOG(LogFTSession, Log, TEXT("[Create] 3) EOS CreateSession 호출 - 콜백 대기"));
	if (!SessionInterface->CreateSession(*UserId, NAME_GameSession, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	UE_LOG(LogFTSession, Log, TEXT("[Create] 4) 콜백 수신: 성공=%s"), bWasSuccessful ? TEXT("true") : TEXT("false"));

	OnCreateSessionCompleteEvent.Broadcast(bWasSuccessful);

	if (bWasSuccessful && bTravelToLobbyOnHost)
	{
		if (UWorld* World = GetWorld())
		{
			const FString LobbyPackage = LobbyLevel.IsNull()
				? TEXT("/Game/Maps/L_Lobby")
				: LobbyLevel.ToSoftObjectPath().GetLongPackageName();
			const FString TravelUrl = LobbyPackage + TEXT("?listen");
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
	// 🌟 로컬 창구 통일 및 동적 갱신
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (Subsystem) SessionInterface = Subsystem->GetSessionInterface();
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr UserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;

	if (!SessionInterface.IsValid() || !UserId.IsValid())
	{
		UE_LOG(LogFTSession, Error, TEXT("[Find] 검증 실패 (로그인 안됨)"));
		OnFindSessionsCompleteEvent.Broadcast(TArray<FFTSessionInfo>(), false);
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxResults);
	SessionSearch->bIsLanQuery = false; 

	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(Key_MatchType, FieryTaleMatchType, EOnlineComparisonOp::Equals);

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	UE_LOG(LogFTSession, Log, TEXT("[Find] EOS FindSessions 호출 - 콜백 대기"));
	if (!SessionInterface->FindSessions(*UserId, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		OnFindSessionsCompleteEvent.Broadcast(TArray<FFTSessionInfo>(), false);
	}
}

void UFTSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

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
			
			if (Info.CurrentPlayers <= 0)
			{
				Info.CurrentPlayers = 1;
			}

			Info.CurrentPlayers = FMath::Clamp(Info.CurrentPlayers, 1, Info.MaxPlayers);
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

	// 🌟 로컬 창구 통일 및 동적 갱신
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (Subsystem) SessionInterface = Subsystem->GetSessionInterface();
	IOnlineIdentityPtr Identity = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr UserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;

	if (!SessionInterface.IsValid() || !SessionSearch.IsValid() || !UserId.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Join] 2) 사전 검증 실패 - 입장 불가"));
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
	if (!SessionInterface->JoinSession(*UserId, NAME_GameSession, Result))
	{
		UE_LOG(LogFTSession, Error, TEXT("[Join] 5-실패) JoinSession 이 즉시 false 반환 - 콜백 없이 종료"));
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		OnJoinSessionCompleteEvent.Broadcast(false);
	}
}

void UFTSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

	UE_LOG(LogFTSession, Log, TEXT("[Join] 6) 콜백 수신: SessionName=%s, Result=%s"),
		*SessionName.ToString(), LexJoinResult(Result));

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);

	if (bSuccess)
	{
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
	// 🌟 동적 갱신
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

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
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

	UE_LOG(LogFTSession, Log, TEXT("[Leave] 3) 콜백 수신: SessionName=%s, 성공=%s (재생성예약=%s, 재입장예약=%s)"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"),
		bCreateSessionOnDestroyComplete ? TEXT("Y") : TEXT("N"),
		bJoinSessionOnDestroyComplete ? TEXT("Y") : TEXT("N"));

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	OnDestroySessionCompleteEvent.Broadcast(bWasSuccessful);

	if (bWasSuccessful && bCreateSessionOnDestroyComplete)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Leave] 4) 파괴 완료 → 보류된 방 생성 진행"));
		bCreateSessionOnDestroyComplete = false;
		CreateSessionInternal(PendingMaxPlayers, PendingRoomName, PendingPassword, bPendingUseLAN);
	}
	else if (bWasSuccessful && bJoinSessionOnDestroyComplete)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Leave] 4) 파괴 완료 → 보류된 입장 재시도"));
		bJoinSessionOnDestroyComplete = false;
		JoinSessionByIndex(PendingJoinIndex, PendingJoinPassword);
	}
}

void UFTSessionSubsystem::StartMatchSession()
{
	// 🌟 동적 갱신
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

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
	if (IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld())) SessionInterface = Subsystem->GetSessionInterface();

	UE_LOG(LogFTSession, Log, TEXT("[Start] 2) 콜백 수신: SessionName=%s, 성공=%s"),
		*SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
}

void UFTSessionSubsystem::LoginEOS(FString InToken)
{
	// 🌟 수정 3: IOnlineSubsystem::Get()을 로컬 창구인 Online::GetSubsystem(GetWorld())로 교체!
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (!Subsystem)
	{
		OnLoginCompleteEvent.Broadcast(false);
		return;
	}

	// 🌟 로컬 창구의 SessionInterface를 최신 상태로 갱신합니다.
	SessionInterface = Subsystem->GetSessionInterface();

	IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		OnLoginCompleteEvent.Broadcast(false);
		return;
	}
	
	if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		// 에픽 게임즈 런처가 켜져있거나 한 경우, 기존 자동 로그인을 강제로 해제합니다!
		if (!InToken.IsEmpty())
		{
			UE_LOG(LogFTSession, Warning, TEXT("[Login] 새로운 토큰(%s)이 입력되어 기존 로그인을 강제 해제합니다."), *InToken);
			Identity->Logout(0);
			// 로그아웃 후, 함수 아래쪽에 있는 Login 로직을 그대로 타게 둡니다.
		}
		else
		{
			// 토큰 입력 없이 그냥 눌렀을 때만 기존 로그인을 유지합니다.
			UE_LOG(LogFTSession, Warning, TEXT("[Login] 이미 로그인되어 있습니다! 바로 성공(True) 처리하고 넘어갑니다."));
			OnLoginCompleteEvent.Broadcast(true);
			return;
		}
	}

	FString AuthType = TEXT("developer");
	FString AuthId = TEXT("localhost:8080"); 
	FString AuthToken = TEXT("Guest"); 

	if (!InToken.IsEmpty())
	{
		AuthToken = InToken;
		UE_LOG(LogFTSession, Warning, TEXT("[Login] 위젯에서 입력받은 토큰을 사용합니다: %s"), *AuthToken);
	}
	else
	{
		// 입력받은 값이 없을 때 커맨드라인 세팅을 확인합니다.
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_LOGIN="), AuthId);
		FParse::Value(FCommandLine::Get(), TEXT("AUTH_PASSWORD="), AuthToken);
	}

	FOnlineAccountCredentials Credentials;
	Credentials.Type = AuthType;
	Credentials.Id = AuthId;
	Credentials.Token = AuthToken;

	Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UFTSessionSubsystem::HandleLoginComplete));
	
	UE_LOG(LogFTSession, Log, TEXT("[Login] EOS 로그인 시도 중... ID: %s, Token: %s"), *AuthId, *AuthToken);
	Identity->Login(0, Credentials);
}

void UFTSessionSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	// 🌟 수정 4: 콜백 처리도 로컬 창구인 Online::GetSubsystem(GetWorld())로 교체!
	IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (Subsystem && Subsystem->GetIdentityInterface().IsValid())
	{
		Subsystem->GetIdentityInterface()->ClearOnLoginCompleteDelegates(0, this);
	}

	UE_LOG(LogFTSession, Log, TEXT("[Login] 로그인 결과: %s, 에러메시지: %s"), bWasSuccessful ? TEXT("성공") : TEXT("실패"), *Error);

	OnLoginCompleteEvent.Broadcast(bWasSuccessful);
}