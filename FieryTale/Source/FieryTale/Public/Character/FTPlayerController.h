// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FTCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "FTPlayerController.generated.h"

class AFTPlayerCharacterBase;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

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
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")                                                                                                                                                                                      
	TSubclassOf<UUserWidget> ArenaHUDWidget;

	UPROPERTY(EditAnywhere, Category = "Setting")
	float RespawnDelay;

	// Input | GamePlay
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LeftClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> RightClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ShiftAction;

	// Input | UI
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> UIMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ToggleHUDEditModeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ScoreboardAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AltMouseAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ChatAction;

protected:
	
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	// 리스폰 타이머 핸들
	FTimerHandle RespawnTimerHandle;
	
	// TODO:: 사망 상태 표시 강조를 위한 임시 위젯 인스턴스. 정식에는 삭제 예정
	UPROPERTY()                                                                                                                                                                                                                       
	TObjectPtr<UUserWidget> DeathOverlayWidgetInstance;
	
	UPROPERTY()                                                                                                                                                                                                                       
	TObjectPtr<UUserWidget> ArenaHUDWidgetInstance;

	// 타이머 만료 후 실제 리스폰을 실행할 함수
	void ExecuteRespawn();

private:
	
	// ASC가 PlayerState에 고정되므로 중복 등록 방지용 핸들 보관
	FDelegateHandle DeadTagEventHandle;

	FVector  RespawnLocation;
	FRotator RespawnRotation;

	// Enhanced Input → Character 위임
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnLeftClick();
	void OnRightClick();
	void OnShift();
	void DebugDie(); // TODO:: 삭제 예정

	// HUD 편집 모드 토글 (F11) — Movable 위젯 위치 조정
	void ToggleHUDEditMode();

	void OnScoreboardPressed();
	void OnAltPressed();
	void OnAltReleased();
	void OnChatPressed();
	
	UFUNCTION()
	void HandleCharacterDeath(AFTCharacterBase* DiedCharacter);

	UPROPERTY()
	TSubclassOf<AFTPlayerCharacterBase> SelectedCharacterClass;
};
