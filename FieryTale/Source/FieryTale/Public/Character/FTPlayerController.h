// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "FTPlayerController.generated.h"

class AFTPlayerCharacterBase;

DECLARE_LOG_CATEGORY_EXTERN(FTPlayerController, Log, All);

UCLASS()
class FIERYTALE_API AFTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// GameMode에서 스폰 위치를 전달받아 PlayerState의 SelectedCharacterClass를 기반으로 캐릭터를 스폰 후 Possess
	void SpawnCharacter(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	// 사용할 캐릭터 넘겨주는 Setter
	// Controller에서 Spawn을 한다는 가정하에 만듬
	void SetSelectedCharacterClass(TSubclassOf<AFTPlayerCharacterBase> InCharacterClass);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

private:
	// Dead 태그 카운트 변화 시 호출 — 사망/부활 분기 처리
	void OnDeadStateChanged(const FGameplayTag Tag, int32 DeadTagCount);

	// HUD 편집 모드 토글 (F11) — Movable 위젯 위치 조정
	void ToggleHUDEditMode();

	UPROPERTY()
	TSubclassOf<AFTPlayerCharacterBase> SelectedCharacterClass;
};
