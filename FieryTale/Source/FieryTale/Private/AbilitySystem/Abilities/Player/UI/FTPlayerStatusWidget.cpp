// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UI//FTPlayerStatusWidget.h"

#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UFTPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBindToOwnerASC();
}

void UFTPlayerStatusWidget::NativeDestruct()
{
	UnbindFromAttributes();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	Super::NativeDestruct();
}

void UFTPlayerStatusWidget::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC || InASC == BoundASC)
	{
		return;
	}

	UnbindFromAttributes();
	BindToAttributes(InASC);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}
}

void UFTPlayerStatusWidget::TryBindToOwnerASC()
{
	if (BoundASC)
	{	//	이미 바인딩됨
		return;
	}

	if (UAbilitySystemComponent* ASC = ResolveOwnerASC())
	{
		BindToAttributes(ASC);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}
		return;
	}

	//	ASC/PlayerState가 아직 준비되지 않음(특히 클라이언트 복제 지연) — 주기적으로 재시도
	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(BindRetryTimer))
		{
			World->GetTimerManager().SetTimer(
				BindRetryTimer, this, &UFTPlayerStatusWidget::TryBindToOwnerASC, 0.25f, true);
		}
	}
}

UAbilitySystemComponent* UFTPlayerStatusWidget::ResolveOwnerASC() const
{
	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn)
	{
		return nullptr;
	}

	//	캐릭터/PlayerState 모두 IAbilitySystemInterface 구현 — Pawn 경유로 PlayerState의 ASC를 얻는다.
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerPawn))
	{
		return ASC;
	}

	if (APlayerState* PS = OwnerPawn->GetPlayerState())
	{
		return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PS);
	}

	return nullptr;
}

void UFTPlayerStatusWidget::BindToAttributes(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	BoundASC = InASC;

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute())
		.AddUObject(this, &UFTPlayerStatusWidget::HandleHealthChanged);
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UFTPlayerStatusWidget::HandleMaxHealthChanged);

	//	초기값 즉시 반영
	bool bFound = false;
	CachedHealth = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetHealthAttribute(), bFound);
	CachedMaxHealth = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetMaxHealthAttribute(), bFound);
	RefreshHealthDisplay();
}

void UFTPlayerStatusWidget::UnbindFromAttributes()
{
	if (!BoundASC)
	{
		return;
	}

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).RemoveAll(this);
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	BoundASC = nullptr;
}

void UFTPlayerStatusWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	RefreshHealthDisplay();
}

void UFTPlayerStatusWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	RefreshHealthDisplay();
}

void UFTPlayerStatusWidget::RefreshHealthDisplay()
{
	if (HealthBar)
	{
		const float Percent = (CachedMaxHealth > 0.f) ? (CachedHealth / CachedMaxHealth) : 0.f;
		HealthBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}

	if (HealthText)
	{
		const FString Display = FString::Printf(TEXT("%d / %d"),
			FMath::RoundToInt(CachedHealth),
			FMath::RoundToInt(CachedMaxHealth));
		HealthText->SetText(FText::FromString(Display));
	}
}
