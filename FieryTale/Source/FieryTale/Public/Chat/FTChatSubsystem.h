// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Chat/FTChatTypes.h"
#include "FTChatSubsystem.generated.h"

class UFTChatComponent;
class AGameModeBase;

// 메시지가 도착할 때마다 발생. 채팅 위젯이 바인딩해 한 줄을 추가한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFTOnChatMessageReceived, const FFTChatMessage&, Message);

/**
 * 채팅의 게임모드 비종속 영속 허브.
 *
 * GameInstance에 붙어 레벨 트래블(메인메뉴 → 로비 → 매치)에도 유지되므로,
 * UI 위젯은 어떤 GameMode/레벨에 있든 이 서브시스템 하나에만 바인딩하면 된다.
 *
 * - 전송: SendMessage() → 로컬 PlayerController의 UFTChatComponent를 통해 서버로
 * - 수신: 컴포넌트가 HandleMessageReceived()로 올려주면 기록에 쌓고 OnMessageReceived 브로드캐스트
 * - 부착: 전역 GameMode PostLogin 이벤트를 받아 서버가 각 컨트롤러에 채팅 컴포넌트를 자동 부착
 */
UCLASS()
class FIERYTALE_API UFTChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** UI/게임플레이에서 채팅을 보낼 때 호출하는 단일 진입점. 로컬 채팅 컴포넌트를 통해 서버로 전달한다. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Chat")
	void SendMessage(const FString& Text, EFTChatChannel Channel = EFTChatChannel::All);

	/** 지금까지 누적된 전체 채팅 기록(All/Team/System 통합, 시각순, 최근 MaxHistory개). 레벨 트래블에도 유지. */
	UFUNCTION(BlueprintPure, Category = "FieryTale|Chat")
	TArray<FFTChatMessage> GetHistory() const;

	/** 특정 채널(All/Team/System)의 기록만 반환한다. 채널별로 독립 보관되며 각자 MaxHistory 상한을 가진다.
	 *  채널 탭(전체/팀/시스템)별 위젯이 자기 채널 기록만 복원/표시할 때 사용한다. */
	UFUNCTION(BlueprintPure, Category = "FieryTale|Chat")
	const TArray<FFTChatMessage>& GetChannelHistory(EFTChatChannel Channel) const;

	/** 기록을 비운다. (예: 새 매치 시작 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Chat")
	void ClearHistory();

	/** 메시지가 도착할 때마다 발생. 채팅 위젯이 바인딩한다. */
	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Chat")
	FFTOnChatMessageReceived OnMessageReceived;

	/** 로컬 System 메시지를 채팅창에 직접 표시한다(네트워크 전송 없음). 로그 서브시스템 등이 사용. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Chat")
	void PushSystemMessage(const FString& Text);

	/** 현재 채팅 위젯이 바인딩되어 있는지(= 채팅창이 떠 있는지). */
	UFUNCTION(BlueprintPure, Category = "FieryTale|Chat")
	bool HasActiveListener() const { return OnMessageReceived.IsBound(); }

	// --- 채팅 컴포넌트 내부 연동용 (일반 호출용 아님) ---

	/** 채팅 컴포넌트가 수신한 메시지를 서브시스템으로 올린다. */
	void HandleMessageReceived(const FFTChatMessage& Message);

	/** 로컬 PlayerController의 채팅 컴포넌트 등록/해제. */
	void RegisterLocalComponent(UFTChatComponent* Component);
	void UnregisterLocalComponent(UFTChatComponent* Component);

protected:
	/** 어떤 GameMode든 PostLogin 시 전역으로 발생 → 서버에서 컨트롤러에 채팅 컴포넌트를 부착한다. */
	void OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

private:
	/** 로컬 송신 경로로 쓸 채팅 컴포넌트(자기 PlayerController의 것). */
	TWeakObjectPtr<UFTChatComponent> LocalChatComponent;

	//	채널별 분리 보관. 각 버킷은 아래 MaxHistory로 독립 상한을 둔다.
	//	→ 한 채널(예: All 채팅) 폭주가 다른 채널(Team/System 알림) 기록을 밀어내지 않는다.
	UPROPERTY()
	TArray<FFTChatMessage> AllHistory;

	UPROPERTY()
	TArray<FFTChatMessage> TeamHistory;

	UPROPERTY()
	TArray<FFTChatMessage> SystemHistory;

	/** Channel에 해당하는 버킷의 가변 참조. HandleMessageReceived의 분리 보관에 쓰인다.
	 *  상수 버전 GetChannelHistory와 동일 매핑을 재사용한다(중복 스위치 방지). */
	TArray<FFTChatMessage>& GetMutableChannelHistory(EFTChatChannel Channel);

	/** 기록 보관 최대 개수(채널 버킷 각각에 적용). 초과분은 앞에서부터 버린다. */
	UPROPERTY(EditAnywhere, Category = "FieryTale|Chat")
	int32 MaxHistory = 100;

	FDelegateHandle PostLoginHandle;
};
