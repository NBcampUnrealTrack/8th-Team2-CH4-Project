// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "Character/FTCharacterTypes.h"
#include "FTTeamPlayerStart.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API AFTTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()
	
public:
	// 에디터에서 이 스폰 지점이 어느 팀 소속인지 설정합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Spawn")
	EFTTeam SpawnTeam;
	
};
