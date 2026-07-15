// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/UI/FTDeathWidget.h"
#include "AbilitySystem/Abilities/Player/UI/FTCircularProgressWidget.h"
#include "Character/FTPlayerController.h"
#include "GameFramework/GameStateBase.h"

void UFTDeathWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!RespawnGauge)
	{
		return;
	}

	AFTPlayerController* PC = Cast<AFTPlayerController>(GetOwningPlayer());
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!PC || !GameState)
	{
		return;
	}

	const float Remaining = FMath::Max(0.f, PC->GetRespawnAvailableTime() - GameState->GetServerWorldTimeSeconds());
	const int32 RemainingSeconds = FMath::CeilToInt(Remaining);

	const float RespawnDelay = PC->RespawnDelay;
	const float Percent = RespawnDelay > 0.f ? (Remaining / RespawnDelay) : 0.f;
	RespawnGauge->SetPercent(Percent);
	RespawnGauge->SetCenterText(FText::AsNumber(RemainingSeconds));
}
