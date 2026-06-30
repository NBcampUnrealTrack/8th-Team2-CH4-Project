// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FTCharacterBase.h"
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
	AFTPlayerController(const FObjectInitializer& Initializer);
	
	// GameMode에서 스폰 위치를 전달받아 PlayerState의 SelectedCharacterClass를 기반으로 캐릭터를 스폰 후 Possess
	void SpawnCharacter(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	// 사용할 캐릭터 넘겨주는 Setter
	// Controller에서 Spawn을 한다는 가정하에 만듬
	void SetSelectedCharacterClass(TSubclassOf<AFTPlayerCharacterBase> InCharacterClass);
	
	// 캐릭터의 Die()에서 서버 권한일 때 호출하는 리스폰 요청 함수
	void RequestRespawn();

	// 캐릭터 사망 시 UI를 끄거나 입력 모드를 변경하기 위해 호출되는 함수
	void OnPlayerDeath();
	
	// TODO:: 사망 상태 표시 강조를 위한 임시 위젯 클래스. 정식에는 삭제 예정                                                                                                                                                                                                           
	UPROPERTY(EditDefaultsOnly, Category = "UI")                                                                                                                                                                                      
	TSubclassOf<UUserWidget> DeathOverlayClass;
	

	UPROPERTY(EditAnywhere, Category = "Setting")
	float RespawnDelay;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	// 리스폰 타이머 핸들
	FTimerHandle RespawnTimerHandle;
	
	// TODO:: 사망 상태 표시 강조를 위한 임시 위젯 인스턴스. 정식에는 삭제 예정
	UPROPERTY()                                                                                                                                                                                                                       
	TObjectPtr<UUserWidget> DeathOverlayWidgetInstance;
	

	// 타이머 만료 후 실제 리스폰을 실행할 함수
	void ExecuteRespawn();

private:
	
	// ASC가 PlayerState에 고정되므로 중복 등록 방지용 핸들 보관
	FDelegateHandle DeadTagEventHandle;

	FVector  RespawnLocation;
	FRotator RespawnRotation;

	// HUD 편집 모드 토글 (F11) — Movable 위젯 위치 조정
	void ToggleHUDEditMode();
	
	UFUNCTION()
	void HandleCharacterDeath(AFTCharacterBase* DiedCharacter);

	UPROPERTY()
	TSubclassOf<AFTPlayerCharacterBase> SelectedCharacterClass;
};
