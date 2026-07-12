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

struct FFTCharacterData;
class UFTHUDLayoutSubsystem;
struct FInputActionValue;

enum class EFTTeam : uint8;

DECLARE_LOG_CATEGORY_EXTERN(FTPlayerController, Log, All);

// 로컬 소유 클라이언트에서 이 컨트롤러의 PlayerState 참조가 막 유효해졌을 때 1회 발생한다.
// PlayerState가 아직 없어 팀 정보를 조회할 수조차 없었던 체력바 위젯들이 구독해뒀다가 재시도하는 용도.
DECLARE_MULTICAST_DELEGATE(FOnPlayerStateReady);

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
	void RequestRespawn();

	// 로컬 소유 클라이언트에서 PlayerState 참조가 막 유효해지면 1회 발생한다
	FOnPlayerStateReady OnPlayerStateReady;

	// DT_CharacterData 테이블 하나만 보관한다. RowName은 EFTCharacterType 값 이름(RedHood/Aladdin/Kaguya/Alice)과
	// 반드시 일치해야 하며, 스폰 시 SpawnCharacter()에서 RowName을 동적으로 찾아 행을 주입한다.
	UPROPERTY(EditDefaultsOnly, Category = "Character")
	TObjectPtr<UDataTable> CharacterDataTable;

	// 실제로 스폰할 캐릭터 클래스. BP 서브클래스(BP_FTPlayerCharacterBase 등)를 지정하면 Turret처럼
	// BP Details 패널에서 컴포넌트/위젯 클래스를 코드 수정 없이 구성할 수 있다.
	// 비어있으면 SpawnCharacter()에서 AFTPlayerCharacterBase::StaticClass()로 폴백한다.
	UPROPERTY(EditDefaultsOnly, Category = "Character")
	TSubclassOf<AFTPlayerCharacterBase> CharacterClassToSpawn;

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

	// HUD 편집 모드 토글 (F11) — Movable 위젯 위치 조정
	void ToggleHUDEditMode();

	// LocalPlayer의 HUD 레이아웃 서브시스템 취득 (편집 모드 On/Off·조회 공용). 없으면 nullptr.
	UFTHUDLayoutSubsystem* GetHUDLayoutSubsystem() const;

	void OnScoreboardPressed();
	// Alt 홀드 = 임시 HUD 편집 모드 (커서 노출 + Movable 위젯 편집 활성 / 해제 시 원복)
	void OnAltPressed();
	void OnAltReleased();
	void OnChatPressed();

	UFUNCTION()
	void HandleCharacterDeath(AFTCharacterBase* DiedCharacter, AController* KillerController);
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
	
	// 서버가 클라이언트에게 결과창을 띄우라고 지시하는 RPC
	UFUNCTION(Client, Reliable)
	void Client_ShowResultUI(uint8 WinningTeam);

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerStartMatch();
	
	UFUNCTION(Server, Reliable)
	void ServerSetCharacter(EFTCharacterType NewCharacter);
	
	// 결과창 위젯 (에디터에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> ResultWidgetClass;

	UPROPERTY()
	UUserWidget* ResultWidgetInstance;
};
