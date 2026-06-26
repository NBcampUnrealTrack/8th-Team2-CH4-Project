// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"

// 초기 게임 시작할 때, 사망 후 부활시 World에 캐릭터를 재배치한다고 가정함. 
// 이때, PlayerState에 이미 플레이어가 어떤 캐릭터를 설정할지가 선행적으로 세팅되어야 함.
// 하지만, PlayerState가 세팅되었다는 가정하에 함수를 호출해야 한다는 자체가 번거롭기 때문에 확정된 구조는 아니고 고민중임.
// Character Spawn위치를 누가 가지고 있어야 하는가, 일단 Controller쪽은 아니고 외부에 있다고 가정함
void AFTPlayerController::SpawnCharacter(const FVector& LocationSpawn, const FRotator& SpawnRotation = FRotator::ZeroRotator)
{
	// 서버 권위에서만 실행되게
	if (!HasAuthority())
	{
		return;
	}

	AFTPlayerState* CurrentFTPlayerState = GetPlayerState<AFTPlayerState>();
	if (!CurrentFTPlayerState || !CurrentFTPlayerState->SelectedCharacterClass)
	{
		return;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	
	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(CurrentFTPlayerState->SelectedCharacterClass, LocationSpawn, SpawnRotation, SpawnParams);
	if (NewPawn)
	{
		Possess(NewPawn);
	}
}

void AFTPlayerController::SetSelectedCharacter(TSubclassOf<AFTPlayerCharacterBase> NewCharacterClass)
{
	ServerSetSelectedCharacter(NewCharacterClass);
}

void AFTPlayerController::ServerSetSelectedCharacter_Implementation(TSubclassOf<AFTPlayerCharacterBase> NewCharacterClass)
{
	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	if (!PS) return;

	PS->SelectedCharacterClass = NewCharacterClass;
}

void AFTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// TODO:: 임시 키 할당
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AFTPlayerController::ReleaseCharacterBase);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AFTPlayerController::BindingCharacterBase);
}

void AFTPlayerController::ReleaseCharacterBase()
{
	if (GetPawn() == nullptr || GetPawn() == PossessedCharacter)
	{
		return;
	}

	// TODO:: Pawn분리 임시 처리
	PossessedCharacter = GetPawn();
	UnPossess();
	ChangeState(NAME_Spectating);
}

void AFTPlayerController::BindingCharacterBase()
{
	if (PossessedCharacter == nullptr)
	{
		return;
	}

	// CharacterBase로 변경
	ChangeState(NAME_Playing);
	Possess(PossessedCharacter);
	PossessedCharacter = nullptr;
}
