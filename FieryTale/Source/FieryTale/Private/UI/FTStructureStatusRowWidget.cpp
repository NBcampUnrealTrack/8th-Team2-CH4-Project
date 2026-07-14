// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FTStructureStatusRowWidget.h"
#include "Level/FTTowerBase.h"
#include "Level/FTNexus.h"
#include "Level/FTTurret.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameplayTags/FTTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UFTStructureStatusRowWidget::NativeDestruct()
{
	UnbindFromStructure();
	Super::NativeDestruct();
}

void UFTStructureStatusRowWidget::InitializeWithStructure(AFTTowerBase* InStructure)
{
	if (!InStructure || InStructure == BoundStructure)
	{
		return;
	}

	UnbindFromStructure();
	BindToStructure(InStructure);
}

void UFTStructureStatusRowWidget::BindToStructure(AFTTowerBase* InStructure)
{
	UAbilitySystemComponent* ASC = InStructure ? InStructure->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	BoundStructure = InStructure;

	if (HealthBar && !DynamicFillMaterial && HealthBar->GetBrush().GetResourceObject() != nullptr)
	{
		DynamicFillMaterial = HealthBar->GetDynamicMaterial();
	}

	ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).AddUObject(this, &UFTStructureStatusRowWidget::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UFTStructureStatusRowWidget::HandleMaxHealthChanged);
	BoundStructure->OnStructureDestroyed.AddDynamic(this, &UFTStructureStatusRowWidget::HandleStructureDestroyed);

	if (const AFTTurret* Turret = Cast<AFTTurret>(BoundStructure))
	{
		if (StructureNameText)
		{
			const bool bIsRightLine = Turret->GetTurretPosition() == EFTTurretPosition::Right;
			StructureNameText->SetText(FText::FromString(bIsRightLine ? TEXT("TOWER R") : TEXT("TOWER L")));
		}
	}
	else if (Cast<AFTNexus>(BoundStructure))
	{
		if (StructureNameText)
		{
			StructureNameText->SetText(FText::FromString(TEXT("NEXUS")));
		}

		ASC->RegisterGameplayTagEvent(FTTags::FTStates::Buff::Invincible, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UFTStructureStatusRowWidget::HandleVulnerabilityTagChanged);
	}

	bool bFound = false;
	CachedHealth = ASC->GetGameplayAttributeValue(UFT_AttributeSet::GetHealthAttribute(), bFound);
	CachedMaxHealth = ASC->GetGameplayAttributeValue(UFT_AttributeSet::GetMaxHealthAttribute(), bFound);

	RefreshHealthDisplay();
	RefreshLockDisplay();

	if (BoundStructure->IsDestroyed())
	{
		HandleStructureDestroyed();
	}

	// 리모트 클라이언트에서 구조물의 BeginPlay(AttributeSet Init)보다 이 바인딩이 먼저 실행되는 경우를
	// 대비해 한 틱 뒤 재조회한다 — 그 시점이면 같은 월드의 모든 액터 BeginPlay가 이미 끝나있다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UFTStructureStatusRowWidget::RefetchInitialAttributes));
	}
}

void UFTStructureStatusRowWidget::UnbindFromStructure()
{
	if (!BoundStructure)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = BoundStructure->GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FTTags::FTStates::Buff::Invincible, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}

	BoundStructure->OnStructureDestroyed.RemoveDynamic(this, &UFTStructureStatusRowWidget::HandleStructureDestroyed);
	BoundStructure = nullptr;
}

void UFTStructureStatusRowWidget::RefetchInitialAttributes()
{
	UAbilitySystemComponent* ASC = BoundStructure ? BoundStructure->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	bool bFound = false;
	CachedHealth = ASC->GetGameplayAttributeValue(UFT_AttributeSet::GetHealthAttribute(), bFound);
	CachedMaxHealth = ASC->GetGameplayAttributeValue(UFT_AttributeSet::GetMaxHealthAttribute(), bFound);

	RefreshHealthDisplay();
	RefreshLockDisplay();
}

void UFTStructureStatusRowWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	RefreshHealthDisplay();
}

void UFTStructureStatusRowWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	RefreshHealthDisplay();
}

void UFTStructureStatusRowWidget::HandleVulnerabilityTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshLockDisplay();
}

void UFTStructureStatusRowWidget::HandleStructureDestroyed()
{
	SetRenderOpacity(0.4f);

	if (LockIcon)
	{
		LockIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshHealthDisplay(); 
}

void UFTStructureStatusRowWidget::RefreshHealthDisplay()
{
	const float SafeMaxHealth = (CachedMaxHealth > 0.f) ? CachedMaxHealth : 1.f;
	const float Percent = FMath::Clamp(CachedHealth / SafeMaxHealth, 0.f, 1.f);

	if (HealthBar && DynamicFillMaterial)
	{
		DynamicFillMaterial->SetScalarParameterValue(TEXT("Percent"), Percent);
	}

	if (HealthText)
	{
		const AFTNexus* Nexus = Cast<AFTNexus>(BoundStructure);

		if (BoundStructure && BoundStructure->IsDestroyed())
		{
			HealthText->SetText(FText::FromString(TEXT("BREAK DOWN")));
		}
		else if (Nexus && !Nexus->IsVulnerable())
		{
			HealthText->SetText(FText::FromString(TEXT("LOCKED")));
		}
		else
		{
			HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Percent * 100.f))));
		}
	}
}

void UFTStructureStatusRowWidget::RefreshLockDisplay()
{
	const AFTNexus* Nexus = Cast<AFTNexus>(BoundStructure);
	const bool bIsLocked = Nexus && !Nexus->IsVulnerable();

	if (LockIcon)
	{
		LockIcon->SetVisibility(bIsLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (HealthBar)
	{
		HealthBar->SetVisibility(bIsLocked ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	RefreshHealthDisplay();
}
