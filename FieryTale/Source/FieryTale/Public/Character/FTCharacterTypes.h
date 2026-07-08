// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FTCharacterTypes.generated.h"

/**
 * FieryTale의 정본(canonical) 캐릭터 타입 Enum.
 *
 * 로비 선택(AFTLobbyPlayerState), 진열대 프리뷰(AFTCharacterDisplayStand),
 * 매치 스폰(AFTPlayerState / AFTPlayerController / AFTGameMode)이 모두 이 하나의 타입을 공유한다.
 * (이전에 EFTCharacterType / EFTGameCharacterType 로 이원화돼 있던 것을 통합함)
 */
UENUM(BlueprintType)
enum class EFTCharacterType : uint8
{
	None    UMETA(DisplayName = "None"),
	RedHood UMETA(DisplayName = "RedHood"),
	Aladdin UMETA(DisplayName = "Aladdin"),
	Kaguya  UMETA(DisplayName = "Kaguya"),
	Alice   UMETA(DisplayName = "Alice")
};

UENUM(BlueprintType)
enum class EFTTeam : uint8
{
	Blue UMETA(DisplayName = "Blue Team"),
	Red  UMETA(DisplayName = "Red Team"),
};
