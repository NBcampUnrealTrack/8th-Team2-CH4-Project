// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Character/FTPlayerState.h"
#include "Character/FTCharacterTypes.h" // 정본 EFTCharacterType 참조
#include "FTLobbyPlayerState.generated.h"

// 대기방 목록 UI가 바인딩해서 다시 그리도록 알리는 이벤트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFTOnLobbyReadyStateChanged);
// 상대방의 캐릭터 선택 상태가 변화했을 때의 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFTOnCharacterStateChanged);

/**
 * 대기방에서 각 플레이어의 준비(Ready) 상태를 복제하는 PlayerState.
 *
 * bIsReady 는 서버 권한으로만 바뀌고, 모든 클라이언트에 복제되어
 * 대기방 위젯이 "누가 준비됐는지"를 표시할 수 있게 한다.
 */

UCLASS()
class FIERYTALE_API AFTLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AFTLobbyPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "FieryTale|Lobby")
	bool IsReady() const { return bIsReady; }

	/** 서버 권한에서만 호출한다. */
	void SetReady(bool bInReady);

	/** Ready 상태가 바뀔 때(서버 직접 변경 또는 클라 OnRep) 발생. UI가 목록을 갱신할 때 바인딩한다. */
	UPROPERTY(BlueprintAssignable, Category = "FieryTale|Lobby")
	FFTOnLobbyReadyStateChanged OnReadyStateChanged;
	
	// 캐릭터 선택이 바뀔 때
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FFTOnCharacterStateChanged OnCharacterStateChanged;

	// 플레이어 진열대 자리 번호KO
	UPROPERTY(Replicated)
	int32 PlayerIndex = -1;
	
	// 캐릭터 변경 요청 및 조회
	void SetCharacterType(EFTCharacterType InCharacterType);
	
	EFTCharacterType GetCharacterType() const { return SelectedCharacterType; }
	
protected:
	
	// 트래블 시 다른 PlayerState로 데이터를 넘겨주는 엔진 내장 함수
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UPROPERTY(ReplicatedUsing = OnRep_IsReady, BlueprintReadOnly, Category = "FieryTale|Lobby")
	bool bIsReady = false;

	UFUNCTION()
	void OnRep_IsReady();
	
	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacter)
	EFTCharacterType SelectedCharacterType;
	
	UFUNCTION()
	void OnRep_SelectedCharacter();
};
