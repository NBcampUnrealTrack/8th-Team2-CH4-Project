// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"
#include "UI/FTHUDLayoutSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

DEFINE_LOG_CATEGORY(FTPlayerController);

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

void AFTPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	if (!PS)
	{
		return;
	}

	PS->SpawnedCharacter = InPawn;

	// TODO: 캐릭터 사망관련 상태가 될 경우,  
	// UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	// if (ASC)
	// {
	// 	ASC->RegisterGameplayTagEvent(FTTags::FTStates::Dead, EGameplayTagEventType::NewOrRemoved)
	// 		.AddUObject(this, &AFTPlayerController::OnDeadStateChanged);
	// }
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

void AFTPlayerController::OnDeadStateChanged(const FGameplayTag Tag, int32 DeadTagCount)
{
	if (!HasAuthority())
	{
		return;
	}

	if (DeadTagCount > 0)
	{
		// 사망 상태 진입
		// - 현재 Pawn 파괴 또는 비활성화
		// - SetViewTarget으로 시야 전환 (팀원 Pawn, 관전 카메라, 구조물 등 기획 미확정)
	}
	else
	{
		// 부활 — Dead GE Duration 만료 시 도달
		// 부활시 배치를 하는 게 맞는가? 그냥 안쓰는 공간에 BaseCharacter를 박아두고 걔의 위치를 옮긴 다음 거기에 빙의시켜 재사용하는 쪽이 맞지 않을까?
		// TODO: GameMode에서 스폰 위치를 받아올 예정
		// SpawnCharacter(SpawnLocation, SpawnRotation);
	}
}
