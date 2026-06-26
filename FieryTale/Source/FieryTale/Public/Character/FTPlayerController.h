// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FTPlayerController.generated.h"

class AFTPlayerCharacterBase;

UCLASS()
class FIERYTALE_API AFTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// TODO:: 확정 X
	// GameMode에서 스폰 위치를 전달받아 PlayerState의 SelectedCharacterClass를 기반으로 캐릭터를 스폰 후 Possess
	void SpawnCharacter(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	// TODO:: 확정 X
	// 클라이언트에서 캐릭터 선택 시 호출 → 서버 PlayerState에 반영
	UFUNCTION(BlueprintCallable, Category = "Character")
	void SetSelectedCharacter(TSubclassOf<AFTPlayerCharacterBase> NewCharacterClass);

protected:
	virtual void SetupInputComponent() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetSelectedCharacter(TSubclassOf<AFTPlayerCharacterBase> NewCharacterClass);

private:
	// TODO:: 삭제 예정
	void ReleaseCharacterBase();  // F: pawn에서 분리
	void BindingCharacterBase();  // V: pawn와 결합

	// 분리 전 캐릭터를 저장해두기 위한 포인터
	UPROPERTY()
	TObjectPtr<APawn> PossessedCharacter = nullptr;
};
