// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"
#include "UI/FTHUDLayoutSubsystem.h"
#include "Character/FTCharacterBase.h"
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
   

DEFINE_LOG_CATEGORY(FTPlayerController);

AFTPlayerController::AFTPlayerController(const FObjectInitializer& Initializer)
	:Super(Initializer)
{
	RespawnDelay = 3.0f;
}

void AFTPlayerController::SpawnCharacter(const FVector& LocationSpawn, const FRotator& SpawnRotation)
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

	const TSoftObjectPtr<UFT_CharacterData>* SoftData = CharacterDataMap.Find(PS->GetSelectedCharacterType());
	if (!SoftData)
	{
		UE_LOG(FTPlayerController, Error, TEXT("CharacterDataMap에 등록되지 않은 타입: %d"), static_cast<int32>(PS->GetSelectedCharacterType()));
		return;
	}

	UFT_CharacterData* CharData = SoftData->LoadSynchronous();
	if (!CharData)
	{
		UE_LOG(FTPlayerController, Error, TEXT("CharacterData 로드 실패"));
		return;
	}

	// Deferred Spawn: BeginPlay 이전에 CharacterData 주입
	const FTransform SpawnTransform(SpawnRotation, LocationSpawn);
	AFTPlayerCharacterBase* NewChar = GetWorld()->SpawnActorDeferred<AFTPlayerCharacterBase>(
		AFTPlayerCharacterBase::StaticClass(),
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (NewChar)
	{
		NewChar->CharacterData = CharData;
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

	// 메인메뉴/로비 컨트롤러가 설정한 FInputModeUIOnly는 GameViewportClient에 남아
	// 트래블 후에도 게임 입력을 차단하므로, 매치 진입 시 게임 입력 모드로 복구한다.
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;

	if (ArenaHUDWidget)
	{
		ArenaHUDWidgetInstance = CreateWidget<UUserWidget>(this, ArenaHUDWidget);
		if (ArenaHUDWidgetInstance)
		{
			ArenaHUDWidgetInstance->AddToViewport();
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

	PS->SpawnedCharacter = InPawn;

	RespawnLocation = InPawn->GetActorLocation();
	RespawnRotation = InPawn->GetActorRotation();
	
	AFTPlayerCharacterBase* TargetCharacter = Cast<AFTPlayerCharacterBase>(InPawn);
	if (TargetCharacter)
	{
		// 캐릭터의 사망 델리게이트에 컨트롤러의 기능을 바인딩
		TargetCharacter->OnCharacterDied.AddDynamic(this, &AFTPlayerController::HandleCharacterDeath);
	}

	if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
	{
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

void AFTPlayerController::OnShift()
{
	if (AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetPawn()))
	{
		Char->OnShift();
	}
}

void AFTPlayerController::DebugDie()
{
	Server_DebugDie();
}

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
	if (DiedCharacter == GetPawn())
	{
		OnPlayerDeath();
		RequestRespawn();
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
