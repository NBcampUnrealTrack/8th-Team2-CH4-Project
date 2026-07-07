// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTCharacterBase.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystem/Abilities/Player/Character/FT_CharacterData.h"
#include "UI/FTHUDLayoutSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTags/FTTags.h"
#include "GameFramework/GameModeBase.h"
#include "Character/FTCharacterTypes.h"

#include "Lobby/FTLobbyPlayerState.h"
#include "Lobby/FTLobbyWidget.h"
#include "FieryTaleLog.h"
#include "Lobby/FTLobbyGameMode.h"

DEFINE_LOG_CATEGORY(FTPlayerController);

AFTPlayerController::AFTPlayerController(const FObjectInitializer& Initializer)
	:Super(Initializer)
{
	RespawnDelay = 3.0f;
}

void AFTPlayerController::SpawnCharacter(const FVector& InSpawnLocation, const FRotator& SpawnRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	if (!PS)
	{
		return;
	}

	// [이관/폐기 보존] 구: 소프트 UFT_CharacterData 에셋을 찾아 동기 로드.
	//const TSoftObjectPtr<UFT_CharacterData>* SoftData = CharacterDataMap.Find(PS->GetSelectedCharacterType());
	//if (!SoftData) { ... return; }
	//UFT_CharacterData* CharData = SoftData->LoadSynchronous();
	//if (!CharData) { ... return; }

	const FDataTableRowHandle* RowHandle = CharacterDataMap.Find(PS->GetSelectedCharacterType());
	if (!RowHandle || RowHandle->IsNull())
	{
		UE_LOG(FTPlayerController, Error, TEXT("CharacterDataMap에 등록되지 않은/유효하지 않은 타입: %d"), static_cast<int32>(PS->GetSelectedCharacterType()));
		return;
	}

	// Deferred Spawn: BeginPlay 이전에 CharacterRow 주입
	const FTransform SpawnTransform(SpawnRotation, InSpawnLocation);
	AFTPlayerCharacterBase* NewChar = GetWorld()->SpawnActorDeferred<AFTPlayerCharacterBase>(
		AFTPlayerCharacterBase::StaticClass(),
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (NewChar)
	{
		//	[이관] NewChar->CharacterData = CharData;
		NewChar->CharacterRow = *RowHandle;
		NewChar->FinishSpawning(SpawnTransform);
		Possess(NewChar);
	}
}

void AFTPlayerController::AssignTeam(EFTTeam InTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	if (!PS)
	{
		return;
	}

	PS->AssignTeamTag(InTeam);
}

void AFTPlayerController::OnPlayerDeath()
{
	if (!IsLocalController())
	{
		return;
	}

	if (DeathOverlayClass)
	{
		DeathOverlayWidgetInstance = CreateWidget<UUserWidget>(this, DeathOverlayClass);
		if (DeathOverlayWidgetInstance)
		{
			DeathOverlayWidgetInstance->AddToViewport();
		}
	}
}

void AFTPlayerController::RequestRespawn()
{
	// 서버권한에서만 동작함
	if (!HasAuthority())
	{
		return;
	}
	
	// 이미 작동중인 리스폰 타이머가 있는 경우 중복 실행 방지
	if (GetWorld()->GetTimerManager().IsTimerActive(RespawnTimerHandle))
	{
		return;
	}
	
	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle, 
		this, 
		&AFTPlayerController::ExecuteRespawn, 
		RespawnDelay, 
		false
	);
}

void AFTPlayerController::ExecuteRespawn()
{
	// 서버에서만 처리
	if (!HasAuthority())
	{
		return;
	}

	AFTPlayerCharacterBase* CurrentCharacter = Cast<AFTPlayerCharacterBase>(GetPawn());
	if (CurrentCharacter)
	{
		CurrentCharacter->TeleportTo(RespawnLocation, RespawnRotation);
		CurrentCharacter->Revive();
	}
}

void AFTPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	/** 이제 플레이어 컨트롤러는 SeamlessTravel에 의해 로비맵과 아레나 맵에서 변경없이 그대로 이어집니다 그렇기 때문에 맵에 따른 로직의 분기 처리 **/
	FString MapName = GetWorld()->GetMapName();
	if (MapName.Contains(TEXT("Lobby")) && LobbyWidgetClass)
	{
		// 로비 UI 생성 로직
		UFTLobbyWidget* LobbyUI = CreateWidget<UFTLobbyWidget>(this, LobbyWidgetClass);
		if (LobbyUI)
		{
			LobbyWidgetInstance = LobbyUI;
			LobbyWidgetInstance->AddToViewport();
			
			bShowMouseCursor = true;
			FInputModeUIOnly InputModeData;
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputModeData);

			if (AFTLobbyPlayerState* LobbyPS = GetPlayerState<AFTLobbyPlayerState>())
			{
				LobbyUI->InitWidget(this, LobbyPS);
			}
		}
	}
	
	
}

void AFTPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	if (!PS)
	{
		return;
	}

	// 🌟 [추가]: 방장(호스트)의 아레나 세팅
	if (IsLocalController())
	{
		bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);

		if (ArenaHUDWidget && !ArenaHUDWidgetInstance)
		{
			ArenaHUDWidgetInstance = CreateWidget<UUserWidget>(this, ArenaHUDWidget);
			if (ArenaHUDWidgetInstance) ArenaHUDWidgetInstance->AddToViewport();
		}
	}

	PS->SpawnedCharacter = InPawn;

	RespawnLocation = InPawn->GetActorLocation();
	RespawnRotation = InPawn->GetActorRotation();
	
	AFTPlayerCharacterBase* TargetCharacter = Cast<AFTPlayerCharacterBase>(InPawn);
	if (TargetCharacter)
	{
		// 캐릭터의 사망 델리게이트에 컨트롤러의 기능을 바인딩
		TargetCharacter->OnCharacterDied.AddDynamic(this, &AFTPlayerController::HandleCharacterDeath);
	}

	// OnPossess는 서버 권한 인스턴스에서만 실행되므로(호스트는 이걸로 충분),
	// 원격 클라이언트 로컬 인스턴스 몫은 OnRep_Pawn()에서 별도로 등록한다.
	if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
	{
		if (DeadTagEventHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
			   .Remove(DeadTagEventHandle);	
		}
		
		
		DeadTagEventHandle = ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AFTPlayerController::OnDeadTagChanged);
	}
}

// ──► [클라이언트 인프라 동기화] 원격 클라이언트 로컬 인스턴스에서 자기 Pawn 정보가 복제 수신될 때 실행된다.
void AFTPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	if (!PS)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
	{
		if (DeadTagEventHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
			   .Remove(DeadTagEventHandle);	
		}
		
		
		DeadTagEventHandle = ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AFTPlayerController::OnDeadTagChanged);
	}
}

void AFTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// IMC 등록 — Controller 생성 시 한 번만 실행, 리스폰 후에도 유지됨
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		if (UIMappingContext)
		{
			Subsystem->AddMappingContext(UIMappingContext, 1);
		}
	}

	// 레거시 한 줄 바인딩 — IA/IMC 에셋 없이 바로 키 연결
	InputComponent->BindKey(EKeys::K,   IE_Pressed, this, &AFTPlayerController::DebugDie); // TODO:: 삭제 예정

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction,       ETriggerEvent::Triggered, this, &AFTPlayerController::Move);
		EIC->BindAction(LookAction,       ETriggerEvent::Triggered, this, &AFTPlayerController::Look);
		EIC->BindAction(LeftClickAction,  ETriggerEvent::Started,   this, &AFTPlayerController::OnLeftClick);
		EIC->BindAction(RightClickAction, ETriggerEvent::Started,   this, &AFTPlayerController::OnRightClick);
		EIC->BindAction(QAction, ETriggerEvent::Started,   this, &AFTPlayerController::OnPressQ);
		EIC->BindAction(ShiftAction,      ETriggerEvent::Started,   this, &AFTPlayerController::OnShift);
		EIC->BindAction(ToggleHUDEditModeAction, ETriggerEvent::Started,   this, &AFTPlayerController::ToggleHUDEditMode);
		EIC->BindAction(ScoreboardAction,        ETriggerEvent::Started,   this, &AFTPlayerController::OnScoreboardPressed);
		EIC->BindAction(AltMouseAction,          ETriggerEvent::Started,   this, &AFTPlayerController::OnAltPressed);
		EIC->BindAction(AltMouseAction,          ETriggerEvent::Completed, this, &AFTPlayerController::OnAltReleased);
		EIC->BindAction(ChatAction,              ETriggerEvent::Started,   this, &AFTPlayerController::OnChatPressed);
	}

}

void AFTPlayerController::Move(const FInputActionValue& Value)
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->Move(Value);
	}
}

void AFTPlayerController::Look(const FInputActionValue& Value)
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->Look(Value);
	}
}

void AFTPlayerController::OnLeftClick()
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->OnLeftClick();
	}
}

void AFTPlayerController::OnRightClick()
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->OnRightClick();
	}
}

void AFTPlayerController::OnPressQ()
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->OnRightClick();
	}
}

void AFTPlayerController::OnShift()
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->OnShift();
	}
}

#if !UE_BUILD_SHIPPING
void AFTPlayerController::DebugDie()
{
	Server_DebugDie();
}
#endif

void AFTPlayerController::Server_DebugDie_Implementation()
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->DebugDie();
	}
}

void AFTPlayerController::ToggleHUDEditMode()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	if (UFTHUDLayoutSubsystem* Subsystem = LocalPlayer->GetSubsystem<UFTHUDLayoutSubsystem>())
	{
		Subsystem->ToggleEditMode();
	}
}

void AFTPlayerController::HandleCharacterDeath(AFTCharacterBase* DiedCharacter)
{
	// 서버권한에서만 동작함
	if (!HasAuthority())
	{
		return;
	}
	
	if (DiedCharacter == GetPawn())
	{
		// 일정시간 후에 Respawn되도록 핸들러를 추가함
		
		// 이미 작동중인 리스폰 타이머가 있는 경우 중복 실행 방지
		if (GetWorld()->GetTimerManager().IsTimerActive(RespawnTimerHandle))
		{
			return;
		}
	
		GetWorldTimerManager().SetTimer(
			RespawnTimerHandle, 
			this, 
			&AFTPlayerController::ExecuteRespawn, 
			RespawnDelay, 
			false
		);
	
	}
}

void AFTPlayerController::OnScoreboardPressed()
{
	
}

void AFTPlayerController::OnAltPressed()
{
	bShowMouseCursor = true;
	// SetIgnoreLookInput(true); // 카메라 입력 무시
}

void AFTPlayerController::OnAltReleased()
{
	bShowMouseCursor = false;
	// SetIgnoreLookInput(false); // 카메라 입력 다시 받음
}

void AFTPlayerController::OnChatPressed()
{

}

void AFTPlayerController::OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!IsLocalController())
	{
		return;
	}

	if (NewCount > 0)
	{
		if (DeathOverlayClass)
		{
			DeathOverlayWidgetInstance = CreateWidget<UUserWidget>(this, DeathOverlayClass);
			if (DeathOverlayWidgetInstance)
			{
				DeathOverlayWidgetInstance->AddToViewport();
			}
		}
	}
	else
	{
		if (DeathOverlayWidgetInstance)
		{
			DeathOverlayWidgetInstance->RemoveFromParent();
			DeathOverlayWidgetInstance = nullptr;
		}
	}
}


// FTPlayerController에 기존 로비에서 사용하던 기능을 모두 병합
void AFTPlayerController::RequestSetReady(bool bReady)
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 1) 클라 요청 RequestSetReady(%s) → ServerRPC (%s)"),
		bReady ? TEXT("true") : TEXT("false"), *GetName());
	ServerSetReady(bReady);
}

void AFTPlayerController::RequestStartMatch()
{
	UE_LOG(LogFTSession, Log, TEXT("[Ready] 호스트 시작 버튼 RequestStartMatch → ServerRPC (%s)"), *GetName());
	ServerStartMatch();
}

void AFTPlayerController::RequestSetCharacter(EFTCharacterType NewCharacter)
{
	ServerSetCharacter(NewCharacter);
}

void AFTPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	// 1. 로비 맵인 경우: FTLobbyPlayerState로 캐스팅이 성공합니다.
	if (AFTLobbyPlayerState* LobbyPS = GetPlayerState<AFTLobbyPlayerState>())
	{
		if (LobbyWidgetInstance) 
		{
			LobbyWidgetInstance->InitWidget(this, LobbyPS);
		}
	}
	else if (AFTPlayerState* PS = GetPlayerState<AFTPlayerState>())
	{
		// 🌟 [추가]: 클라이언트(접속자)의 아레나 세팅
		if (IsLocalController())
		{
			bShowMouseCursor = false;
			FInputModeGameOnly InputMode;
			SetInputMode(InputMode);

			if (ArenaHUDWidget && !ArenaHUDWidgetInstance)
			{
				ArenaHUDWidgetInstance = CreateWidget<UUserWidget>(this, ArenaHUDWidget);
				if (ArenaHUDWidgetInstance) ArenaHUDWidgetInstance->AddToViewport();
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("[PlayerController] 전장 클라이언트 UI 및 입력 모드 복구 완료!"));
	}
}

void AFTPlayerController::ServerSetReady_Implementation(bool bReady)
{
	// 서버에 있는 플레이어 스테이트와 게임모드에 보내는 RPC
	if (AFTLobbyPlayerState* LobbyPS = GetPlayerState<AFTLobbyPlayerState>())
	{
		LobbyPS->SetReady(bReady);
	}
	if (AFTLobbyGameMode* GameMode = GetWorld()->GetAuthGameMode<AFTLobbyGameMode>())
	{
		GameMode->NotifyReadyStateChanged();
	}
}

void AFTPlayerController::ServerStartMatch_Implementation()
{
	if (AFTLobbyGameMode* GameMode = GetWorld()->GetAuthGameMode<AFTLobbyGameMode>())
	{
		GameMode->RequestStartMatch(this);
	}
}

void AFTPlayerController::ServerSetCharacter_Implementation(EFTCharacterType NewCharacter)
{
	if (AFTLobbyPlayerState* LobbyPS = GetPlayerState<AFTLobbyPlayerState>())
	{
		LobbyPS->SetCharacterType(NewCharacter);
	}
}
