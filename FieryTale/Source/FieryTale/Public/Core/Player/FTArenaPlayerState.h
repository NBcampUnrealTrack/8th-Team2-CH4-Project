// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/FTLobbyPlayerState.h"
#include "FTArenaPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API AFTArenaPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:	
	
	AFTArenaPlayerState();

	// 네트워크 복제(Replication)를 위해 반드시 필요한 함수 선언
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void CopyProperties(APlayerState* PlayerState);

	// 로비에서 넘겨받을 캐릭터 정보
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character")
	EFTCharacterType SelectedCharacter;
	
	
};
