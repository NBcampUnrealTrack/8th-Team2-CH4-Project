// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Player/FTArenaPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Core/Game/FTGameMode.h"

AFTArenaPlayerState::AFTArenaPlayerState()
{
	SelectedCharacter = EFTCharacterType::None;
	bReplicates = true; // PlayerState의 복제 활성화
}

void AFTArenaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AFTArenaPlayerState, SelectedCharacter);
}

void AFTArenaPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
}