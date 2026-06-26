// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FTLobbyPlayerController.generated.h"

/**
 * 대기방 전용 PlayerController.
 *
 * UI(대기방 위젯)는 RequestSetReady / RequestStartMatch 만 호출하면 되고,
 * 실제 상태 변경은 ServerRPC 를 통해 서버 권한으로 처리된다.
 */
UCLASS()
class FIERYTALE_API AFTLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 자기 Ready 상태 변경 요청. (소유 클라이언트에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestSetReady(bool bReady);

	/** 매치 시작 요청. (대기방 호스트 버튼에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestStartMatch();

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerStartMatch();
};
