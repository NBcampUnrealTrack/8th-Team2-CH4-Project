// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FTLobbyPlayerController.generated.h"

enum class EFTCharacterType :  uint8;
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
protected:
	virtual void BeginPlay() override;

	virtual void OnRep_PlayerState() override;
	
public:
	/** 자기 Ready 상태 변경 요청. (소유 클라이언트에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestSetReady(bool bReady);

	/** 매치 시작 요청. (대기방 호스트 버튼에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestStartMatch();
	
	// UI에서 캐릭터 선택할 시 호출되는 함수
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestSetCharacter(EFTCharacterType NewCharacter);
	
	// 로비 위젯 변수
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> LobbyWidgetClass;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerStartMatch();
	
	UFUNCTION(Server, Reliable)
	void ServerSetCharacter(EFTCharacterType NewCharacter);
	
	// 로비 위젯 인스턴스
	UPROPERTY()
	class UFTLobbyWidget* LobbyWidgetInstance;
};
