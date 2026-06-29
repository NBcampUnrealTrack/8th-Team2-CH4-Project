// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FTMainMenuPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API AFTMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
	
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UFTMainMenuWidget> MainMenuWidgetClass;
};
