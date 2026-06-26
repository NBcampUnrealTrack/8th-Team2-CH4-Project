// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "FTPlayerState.generated.h"

class AFTPlayerCharacterBase;

UCLASS()
class FIERYTALE_API AFTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 플레이어가 선택한 캐릭터 가정
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Character")
	TSubclassOf<AFTPlayerCharacterBase> SelectedCharacterClass;
};
