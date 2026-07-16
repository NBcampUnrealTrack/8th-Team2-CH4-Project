// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "FTSessionSubsystem.generated.h"

class UWorld;

/**
 * 세션 검색 결과를 UMG(Blueprint)에서 다룰 수 있게 만든 표시용 구조체.
 * 엔진의 FOnlineSessionSearchResult는 BP에 노출되지 않으므로, 목록 UI에 필요한 값만 추려서 전달한다.
 */
USTRUCT(BlueprintType)
struct FFTSessionInfo
{
	GENERATED_BODY()

	/** UFTSessionSubsystem 내부 검색 결과 배열의 인덱스. JoinSessionByIndex 에 그대로 넘긴다. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	int32 SearchIndex = -1;

	/** 방 이름 (호스트가 지정한 ROOMNAME). */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	FString RoomName;

	/** 현재 접속 인원. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	int32 CurrentPlayers = 0;

	/** 최대 인원. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	int32 MaxPlayers = 0;

	/** 핑(ms). */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	int32 PingMs = 0;

	/** 비밀번호가 걸린 방인지 여부. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	bool bHasPassword = false;

	/** 방장 표시 이름. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Session")
	FString OwningPlayerName;
};

// UMG 위젯이 바인딩할 결과 이벤트들. (Dynamic Multicast 라서 BlueprintAssignable 가능)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTOnCreateSessionComplete, bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFTOnFindSessionsComplete, const TArray<FFTSessionInfo>&, SessionResults,
                                             bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTOnJoinSessionComplete, bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTOnDestroySessionComplete, bool, bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTOnLoginComplete, bool, bWasSuccessful);

/**
 * 로비/매치메이킹의 세션 계층을 담당하는 서브시스템.
 *
 * OnlineSubsystem(IOnlineSession)을 감싸 방 생성/검색/입장/종료를 처리하고,
 * UMG 위젯이 BlueprintCallable 함수 호출과 BlueprintAssignable 이벤트 바인딩만으로
 * 메인 메뉴/서버 목록 UI를 구성할 수 있게 한다.
 *
 * GameInstance에 부착되어 레벨 트래블(메인 메뉴 -> 대기방 -> 매치) 사이에도 유지된다.
 */
UCLASS()
class FIERYTALE_API UFTSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFTSessionSubsystem();

	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** 방을 만들고(세션 생성) 성공하면 대기방(Lobby) 맵으로 ServerTravel 한다. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Session")
	void HostSession(int32 MaxPlayers = 4, const FString& RoomName = "FieryTale Room", const FString& Password = "",
	                 bool bUseLAN = false);

	/** 방 목록을 검색한다. 완료 시 OnFindSessionsCompleteEvent 로 결과를 전달한다. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Session")
	void FindSessions(int32 MaxResults = 20, bool bUseLAN = false);

	/** 검색 결과 인덱스로 방에 입장한다. 비밀번호가 걸린 방이면 Password 가 일치해야 한다. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Session")
	void JoinSessionByIndex(int32 SearchIndex, const FString& Password = "");

	/** 현재 세션에서 나간다(세션 파괴). */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Session")
	void LeaveSession();
	
	
	
	/** EOS DevAuthTool을 이용해 로그인합니다. (PIE 인덱스에 따라 자동 분기) */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Auth")
	void LoginEOS(FString InToken = TEXT(""));

	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Auth")
	FFTOnLoginComplete OnLoginCompleteEvent;
	
	

	/** (선택) 세션을 InProgress 로 표시한다. 매치 시작 시 호스트가 호출. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Session")
	void StartMatchSession();

	// UMG 바인딩용 이벤트
	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Session")
	FFTOnCreateSessionComplete OnCreateSessionCompleteEvent;

	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Session")
	FFTOnFindSessionsComplete OnFindSessionsCompleteEvent;

	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Session")
	FFTOnJoinSessionComplete OnJoinSessionCompleteEvent;

	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Session")
	FFTOnDestroySessionComplete OnDestroySessionCompleteEvent;

	/** 호스트 성공 시 이동할 대기방 맵. 에디터/BP에서 레벨 에셋을 직접 지정한다.
	 *  비어 있으면 코드 폴백(/Game/Maps/L_Lobby)을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Session")
	TSoftObjectPtr<UWorld> LobbyLevel;

	/** 세션 생성 성공 시 자동으로 대기방 맵으로 ServerTravel 할지 여부. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Session")
	bool bTravelToLobbyOnHost = true;

private:
	// 실제 세션 생성 로직. HostSession 에서 기존 세션 정리 후 호출된다.
	void CreateSessionInternal(int32 MaxPlayers, const FString& RoomName, const FString& Password, bool bUseLAN);
	
	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	// IOnlineSession 콜백
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<class FOnlineSessionSearch> SessionSearch;

	// 콜백 델리게이트와 핸들
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;

	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;

	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;

	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;

	// 기존 세션을 파괴한 뒤 새로 생성해야 할 때 사용하는 보류 값
	bool bCreateSessionOnDestroyComplete = false;
	int32 PendingMaxPlayers = 4;
	FString PendingRoomName;
	FString PendingPassword;
	bool bPendingUseLAN = true;

	// 기존 세션을 파괴한 뒤 다시 입장해야 할 때 사용하는 보류 값
	// (이전 PIE 실행에서 남은 GameSession 때문에 JoinSession 이 "already exists" 로 실패하는 것을 자가 치유)
	bool bJoinSessionOnDestroyComplete = false;
	int32 PendingJoinIndex = -1;
	FString PendingJoinPassword;

	// 세션 설정에 저장하는 커스텀 키
	static const FName Key_MatchType;
	static const FName Key_RoomName;
	static const FName Key_Password;
};
