// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/FTPlayerState.h"
#include "Net/UnrealNetwork.h"

void AFTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTPlayerState, SelectedCharacterClass);
}

