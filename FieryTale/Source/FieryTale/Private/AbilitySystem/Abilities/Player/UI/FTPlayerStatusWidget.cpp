// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UI//FTPlayerStatusWidget.h"

#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Character/FTPlayerState.h"
#include "Engine/Texture2D.h"

void UFTPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (HealthBar && HealthBarMaterial && !HealthMID)
	{
		HealthMID = UMaterialInstanceDynamic::Create(HealthBarMaterial, this);
		HealthBar->SetBrushFromMaterial(HealthMID);
	}
	
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
	
	UpdatePortrait();// 초상화 업데이트

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
		
		UpdatePortrait();
		
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
	const float Percent = (CachedMaxHealth > 0.f) ? (CachedHealth / CachedMaxHealth) : 0.f;
	const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);

	// 🌟 [수정] 기존 HealthBar->SetPercent(...) 대신, 머티리얼의 "Percent" 파라미터로 값을 쏘아줍니다.[cite: 16]
	if (HealthMID)
	{
		HealthMID->SetScalarParameterValue(TEXT("Percent"), ClampedPercent);
	}

	if (HealthText)
	{
		const FString Display = FString::Printf(TEXT("%d / %d"),
			FMath::RoundToInt(CachedHealth),
			FMath::RoundToInt(CachedMaxHealth));
		HealthText->SetText(FText::FromString(Display));
	}
}

void UFTPlayerStatusWidget::UpdatePortrait()
{
	if (!PortraitImage) return;

	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn) return;

	// 오너 폰의 PlayerState를 가져와서 캐스팅합니다.
	if (AFTPlayerState* PS = OwnerPawn->GetPlayerState<AFTPlayerState>())
	{
		// PlayerState에서 캐릭터 타입을 가져옵니다.
		EFTCharacterType CharType = PS->GetSelectedCharacterType();

		// PortraitMap에 해당 캐릭터 타입의 텍스처가 등록되어 있는지 확인합니다.
		if (const TSoftObjectPtr<UTexture2D>* SoftTexture = PortraitMap.Find(CharType))
		{
			// 소프트 포인터를 동기식으로 로드하여 이미지 브러시에 세팅합니다.
			if (UTexture2D* LoadedTexture = SoftTexture->LoadSynchronous())
			{
				PortraitImage->SetBrushFromTexture(LoadedTexture);
			}
		}
	}
}
