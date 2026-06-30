// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"
#include "UI/FTHUDLayoutSubsystem.h"
#include "Character/FTCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Components/InputComponent.h"
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

// TODO:: 삭제 예정
void AFTPlayerController::SpawnCharacter(const FVector& LocationSpawn, const FRotator& SpawnRotation = FRotator::ZeroRotator)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!SelectedCharacterClass)
	{
		UE_LOG(FTPlayerController, Error, TEXT("SelectedCharacterClass가 설정되지 않음"));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(SelectedCharacterClass, LocationSpawn, SpawnRotation, SpawnParams);
	if (NewPawn)
	{
		Possess(NewPawn);
	}
}

void AFTPlayerController::SetSelectedCharacterClass(TSubclassOf<AFTPlayerCharacterBase> InCharacterClass)
{
	SelectedCharacterClass = InCharacterClass;
}

void AFTPlayerController::OnPlayerDeath()
{
	// TODO:: DeadScreen 등 처리
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
	
	if (DeathOverlayWidgetInstance)                                                                                                                                                                                                           
	{                                                                                                                                                                                                                                 
		DeathOverlayWidgetInstance->RemoveFromParent();                                                                                                                                                                                       
		DeathOverlayWidgetInstance = nullptr;                                                                                                                                                                                                 
	}
	
	AFTPlayerCharacterBase* CurrentCharacter = Cast<AFTPlayerCharacterBase>(GetPawn());
	if (CurrentCharacter)
	{
		CurrentCharacter->TeleportTo(RespawnLocation, RespawnRotation);
		CurrentCharacter->Revive();
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
}

void AFTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		// 레거시 키 바인딩 — 추후 Enhanced Input의 InputAction으로 교체 가능
		InputComponent->BindKey(EKeys::F11, IE_Pressed, this, &AFTPlayerController::ToggleHUDEditMode);
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
	// 내가 현재 조종하고 있던 메인 캐릭터가 죽은 게 맞는지 검증
	if (DiedCharacter == GetPawn())
	{
		// 내 메인 캐릭터가 죽었을 때의 UI/입력 처리 실행
		OnPlayerDeath(); 
		RequestRespawn();
	}
	else
	{
		// 죽은 게 메인캐릭터가 아닌 경우?
	}
}