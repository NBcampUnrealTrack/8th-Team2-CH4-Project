// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Character/FTCharacterTypes.h"
#include "FTPlayerController.generated.h"

class AFTCharacterBase;
class AFTPlayerCharacterBase;
class AFTPlayerState;
class UInputMappingContext;
class UInputAction;
class UFTLobbyWidget;
class UFT_CharacterData;
struct FInputActionValue;
enum class EFTTeam : uint8;

DECLARE_LOG_CATEGORY_EXTERN(FTPlayerController, Log, All);

UCLASS()
class FIERYTALE_API AFTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFTPlayerController(const FObjectInitializer& Initializer);

	// GameMode에서 스폰 위치를 전달받아 PlayerState의 SelectedCharacterType 기반으로 캐릭터를 스폰 후 Possess
	void SpawnCharacter(const FVector& InSpawnLocation, const FRotator& SpawnRotation);
	
	// GameMode에서 호출 — 팀을 PlayerState에 바인딩
	void AssignTeam(EFTTeam InTeam);
	void OnPlayerDeath();
	void RequestRespawn();

	// [이관/폐기 보존] 구: EFTCharacterType별 UFT_CharacterData 소프트 에셋 맵.
	//UPROPERTY(EditDefaultsOnly, Category = "Character")
	//TMap<EFTCharacterType, TSoftObjectPtr<UFT_CharacterData>> CharacterDataMap;

	// 신: EFTCharacterType별 DT_CharacterData(FFTCharacterData) 행 핸들 — 스폰 시 캐릭터에 주입한다.
	UPROPERTY(EditDefaultsOnly, Category = "Character", meta = (RowType = "FTCharacterData"))
	TMap<EFTCharacterType, FDataTableRowHandle> CharacterDataMap;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|GamePlay", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> QAction;


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
	virtual void OnRep_Pawn() override;
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
	void OnPressQ();
	void OnShift();
	// TODO:: Debugging 용, 추후 삭제 예정
	
#if !UE_BUILD_SHIPPING
	void DebugDie();
#endif

	UFUNCTION(Server, Reliable)
	void Server_DebugDie();

	// HUD 편집 모드 토글 (F11) — Movable 위젯 위치 조정
	void ToggleHUDEditMode();

	void OnScoreboardPressed();
	void OnAltPressed();
	void OnAltReleased();
	void OnChatPressed();

	UFUNCTION()
	void HandleCharacterDeath(AFTCharacterBase* DiedCharacter);
	void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	
public:
	// --- 로비 UI & 시스템 통합 ---
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI|Lobby")
	TSubclassOf<class UUserWidget> LobbyWidgetClass;

	UPROPERTY()
	TObjectPtr<UFTLobbyWidget> LobbyWidgetInstance;

	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestSetReady(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestStartMatch();

	UFUNCTION(BlueprintCallable, Category = "FieryTale|Lobby")
	void RequestSetCharacter(EFTCharacterType NewCharacter); // Enum 통일

	virtual void OnRep_PlayerState() override;
	void InitializeLobbyLocal();

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerStartMatch();
	
	UFUNCTION(Server, Reliable)
	void ServerSetCharacter(EFTCharacterType NewCharacter);
};
