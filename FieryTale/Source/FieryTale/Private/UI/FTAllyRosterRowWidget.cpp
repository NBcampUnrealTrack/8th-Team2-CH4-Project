// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FTAllyRosterRowWidget.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UFTAllyRosterRowWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PortraitRetryTimerHandle);
	}
	UnbindFromAttributes();
	Super::NativeDestruct();
}

void UFTAllyRosterRowWidget::InitializeWithPlayerState(AFTPlayerState* InPlayerState)
{
	if (!InPlayerState)
	{
		return;
	}

	BoundPlayerState = InPlayerState;
	TryResolvePortrait();
	BindToAttributes(InPlayerState->GetAbilitySystemComponent());
}

void UFTAllyRosterRowWidget::TryResolvePortrait()
{
	if (!BoundPlayerState)
	{
		return;
	}

	const AFTPlayerCharacterBase* Character = Cast<AFTPlayerCharacterBase>(BoundPlayerState->SpawnedCharacter.Get());
	const FFTCharacterData* Data = Character ? Character->GetCharacterData() : nullptr;

	if (Data)
	{
		if (PortraitImage)
		{
			if (UTexture2D* Portrait = Data->PortraitIcon.LoadSynchronous())
			{
				PortraitImage->SetBrushFromTexture(Portrait);
			}
		}
		return;
	}

	// 캐릭터가 아직 스폰 전 — 잠시 후 재시도
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PortraitRetryTimerHandle, this, &UFTAllyRosterRowWidget::TryResolvePortrait, 0.3f, false);
	}
}

void UFTAllyRosterRowWidget::BindToAttributes(UAbilitySystemComponent* InASC)
{
	if (!InASC || InASC == BoundASC)
	{
		FetchInitialAttributes();
		return;
	}

	UnbindFromAttributes();
	BoundASC = InASC;

	if (HealthBar && !DynamicFillMaterial && HealthBar->GetBrush().GetResourceObject() != nullptr)
	{
		DynamicFillMaterial = HealthBar->GetDynamicMaterial();
	}

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).AddUObject(this, &UFTAllyRosterRowWidget::HandleHealthChanged);
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UFTAllyRosterRowWidget::HandleMaxHealthChanged);
	BoundASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UFTAllyRosterRowWidget::OnDeathTagChanged);

	FetchInitialAttributes();
}

void UFTAllyRosterRowWidget::UnbindFromAttributes()
{
	if (!BoundASC)
	{
		return;
	}

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).RemoveAll(this);
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	BoundASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	BoundASC = nullptr;
}

void UFTAllyRosterRowWidget::FetchInitialAttributes()
{
	if (!BoundASC)
	{
		return;
	}

	bool bFound = false;
	const float InitialHealth = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetHealthAttribute(), bFound);
	if (bFound)
	{
		CachedHealth = InitialHealth;
	}

	const float InitialMaxHealth = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetMaxHealthAttribute(), bFound);
	if (bFound)
	{
		CachedMaxHealth = InitialMaxHealth;
	}

	RefreshHealthDisplay();
	OnDeathTagChanged(FTTags::FTStates::Core::Dead, BoundASC->HasMatchingGameplayTag(FTTags::FTStates::Core::Dead) ? 1 : 0);
}

void UFTAllyRosterRowWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	RefreshHealthDisplay();
}

void UFTAllyRosterRowWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	RefreshHealthDisplay();
}

void UFTAllyRosterRowWidget::OnDeathTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bDead = NewCount > 0;
	const ESlateVisibility AliveVisibility = bDead ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;

	if (HealthBar)
	{
		HealthBar->SetVisibility(AliveVisibility);
	}
	if (HealthBarBackdrop)
	{
		HealthBarBackdrop->SetVisibility(AliveVisibility);
	}
	if (HealthBarBorder)
	{
		HealthBarBorder->SetVisibility(AliveVisibility);
	}
	if (HealthText)
	{
		HealthText->SetVisibility(AliveVisibility);
	}
	if (DeadText)
	{
		DeadText->SetVisibility(bDead ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFTAllyRosterRowWidget::RefreshHealthDisplay()
{
	const float SafeMaxHealth = (CachedMaxHealth > 0.f) ? CachedMaxHealth : 100.f;
	const float Percent = FMath::Clamp(CachedHealth / SafeMaxHealth, 0.f, 1.f);

	if (DynamicFillMaterial)
	{
		DynamicFillMaterial->SetScalarParameterValue(TEXT("Percent"), Percent);
	}

	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Percent * 100.f))));
	}
}
