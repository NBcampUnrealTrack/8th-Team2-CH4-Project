// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chat/FTChatTypes.h"
#include "FTChatComponent.generated.h"

class UFTChatSubsystem;

/**
 * 채팅 네트워크 전송 계층. PlayerController에 부착되는 복제 컴포넌트다.
 *
 * - 송신: 로컬 클라이언트 → Server RPC → 서버 검증 → 전체 분배
 * - 수신: 서버 → 소유 클라이언트 Client RPC → 로컬 GameInstance의 UFTChatSubsystem으로 라우팅
 *
 * 특정 GameMode/PlayerController 클래스에 의존하지 않는다. UFTChatSubsystem이 전역
 * GameMode PostLogin 이벤트를 받아 어떤 컨트롤러에든 이 컴포넌트를 자동으로 붙여준다.
 * (BP/C++에서 직접 추가해도 무방하다.)
 */
UCLASS(ClassGroup = (FieryTale), meta = (BlueprintSpawnableComponent))
class FIERYTALE_API UFTChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFTChatComponent();

	/** 로컬 플레이어가 채팅을 보낼 때의 진입점. (소유 클라이언트에서 호출) 서버 RPC로 전달한다. */
	void SendMessage(const FString& Text, EFTChatChannel Channel);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 클라 → 서버: 채팅 전송 요청. 서버가 발신자/내용 검증 후 전체에 분배한다. */
	//	WithValidation 키워드 : RPC에서 함수가 실행되기 전에 유효성 검사를 먼저 진행한다.
	//	함수명_Validate 함수를 구현해야 하며 반환값은 bool이다
	//	해당 함수의 반환값으로 false가 반환될 경우 해당 호출만 막는 것이 아니라 해당 패킷을 보낸 클라이언트 자체를 차단해버린다
	//	그러므로 Validation 함수는 클라이언트에서의 조작 없이는 절대 불가능한 값으로 검증을 해야한다. 
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendMessage(const FString& Text, EFTChatChannel Channel);

	/** 서버 → 소유 클라: 분배된 메시지 1건 수신. 로컬 서브시스템으로 올린다. */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveMessage(const FFTChatMessage& Message);

	/** [서버 전용] 완성된 메시지를 접속 중인 모든 컨트롤러의 채팅 컴포넌트로 분배한다. */
	void ServerDistributeMessage(const FFTChatMessage& Message);

	/** 수신 메시지를 로컬 GameInstance의 채팅 서브시스템으로 라우팅한다. */
	void RouteToSubsystem(const FFTChatMessage& Message) const;

	APlayerController* GetOwningPlayerController() const;
	FString ResolveSenderName() const;
	UFTChatSubsystem* GetChatSubsystem() const;
};
