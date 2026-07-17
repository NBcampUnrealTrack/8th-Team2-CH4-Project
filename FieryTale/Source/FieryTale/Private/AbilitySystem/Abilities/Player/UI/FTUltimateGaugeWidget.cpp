// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UI/FTUltimateGaugeWidget.h"

#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "AbilitySystem/Abilities/Player/Data/FTSkillMetaData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

void UFTUltimateGaugeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	EnsureGaugeMID();
	RefreshGaugeDisplay();
}

void UFTUltimateGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureGaugeMID();
	TryBindToOwnerASC();
}

#if WITH_EDITOR
void UFTUltimateGaugeWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshGaugeDisplay();
}
#endif

void UFTUltimateGaugeWidget::EnsureGaugeMID()
{
	if (GaugeMID || !GaugeBar || !RoundProgressMaterial)
	{
		return;
	}

	GaugeMID = UMaterialInstanceDynamic::Create(RoundProgressMaterial, this);
	GaugeBar->SetBrushFromMaterial(GaugeMID);
}

void UFTUltimateGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsGaugeFull)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Pulse = FMath::MakePulsatingValue(World->GetTimeSeconds(), BlinkPulsesPerSecond);
	SetRenderOpacity(FMath::Lerp(BlinkMinOpacity, 1.0f, Pulse));
}

void UFTUltimateGaugeWidget::NativeDestruct()
{
	UnbindFromASC();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	Super::NativeDestruct();
}

void UFTUltimateGaugeWidget::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC || InASC == BoundASC)
	{
		return;
	}

	UnbindFromASC();
	BindToASC(InASC);

	if (!bSkillMetaResolved)
	{
		RefreshSkillMetaData();
	}

	if (bSkillMetaResolved)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}
	}
}

void UFTUltimateGaugeWidget::TryBindToOwnerASC()
{
	if (!BoundASC)
	{
		if (UAbilitySystemComponent* ASC = ResolveOwnerASC())
		{
			BindToASC(ASC);
		}
	}

	if (!bSkillMetaResolved)
	{
		RefreshSkillMetaData();
	}

	//	ASC 바인딩 + 스킬 메타데이터 확보가 모두 끝났으면 재시도 종료
	if (BoundASC && bSkillMetaResolved)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}
		return;
	}

	//	ASC/PlayerState/CharacterData가 아직 준비되지 않음(특히 클라이언트 복제 지연) — 주기적으로 재시도
	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(BindRetryTimer))
		{
			World->GetTimerManager().SetTimer(
				BindRetryTimer, this, &UFTUltimateGaugeWidget::TryBindToOwnerASC, 0.25f, true);
		}
	}
}

UAbilitySystemComponent* UFTUltimateGaugeWidget::ResolveOwnerASC() const
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

void UFTUltimateGaugeWidget::BindToASC(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	BoundASC = InASC;

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetUltimateGaugeAttribute())
		.AddUObject(this, &UFTUltimateGaugeWidget::HandleUltimateGaugeChanged);
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxUltimateGaugeAttribute())
		.AddUObject(this, &UFTUltimateGaugeWidget::HandleMaxUltimateGaugeChanged);

	//	위젯이 늦게 생성/재바인딩될 수 있으므로(재접속 등) 현재 값으로 즉시 동기화
	bool bFound = false;
	CachedUltimateGauge = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetUltimateGaugeAttribute(), bFound);
	CachedMaxUltimateGauge = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetMaxUltimateGaugeAttribute(), bFound);
	RefreshGaugeDisplay();
}

void UFTUltimateGaugeWidget::UnbindFromASC()
{
	if (!BoundASC)
	{
		return;
	}

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetUltimateGaugeAttribute()).RemoveAll(this);
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxUltimateGaugeAttribute()).RemoveAll(this);
	BoundASC = nullptr;

	//	캐릭터 교체 등으로 소유 폰이 바뀔 수 있으므로, 재바인딩 시 새 캐릭터의 RSkill 메타데이터를 다시 조회하게 한다.
	bSkillMetaResolved = false;
}

void UFTUltimateGaugeWidget::HandleUltimateGaugeChanged(const FOnAttributeChangeData& Data)
{
	CachedUltimateGauge = Data.NewValue;

	UE_LOG(LogTemp, Log, TEXT("[UltimateGauge] %.2f -> %.2f (Max=%.2f)"),
		Data.OldValue, Data.NewValue, CachedMaxUltimateGauge);

	RefreshGaugeDisplay();
}

void UFTUltimateGaugeWidget::HandleMaxUltimateGaugeChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxUltimateGauge = Data.NewValue;

	UE_LOG(LogTemp, Log, TEXT("[UltimateGauge] MaxGauge %.2f -> %.2f (Current=%.2f)"),
		Data.OldValue, Data.NewValue, CachedUltimateGauge);

	RefreshGaugeDisplay();
}

void UFTUltimateGaugeWidget::RefreshGaugeDisplay()
{
	const float Percent = (CachedMaxUltimateGauge > 0.f) ? (CachedUltimateGauge / CachedMaxUltimateGauge) : 0.f;
	const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);

	if (GaugeMID)
	{
		GaugeMID->SetScalarParameterValue(TEXT("Percent"), ClampedPercent);
	}

	const bool bWasFull = bIsGaugeFull;
	bIsGaugeFull = ClampedPercent >= 1.0f;

	//	100%일 때는 퍼센트 텍스트 대신 준비 완료 아이콘을 표시합니다.
	if (GaugeText)
	{
		GaugeText->SetVisibility(bIsGaugeFull ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		if (!bIsGaugeFull)
		{
			GaugeText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(ClampedPercent * 100.f))));
		}
	}

	if (SkillImage)
	{
		SkillImage->SetVisibility(bIsGaugeFull ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (bWasFull && !bIsGaugeFull)
	{
		//	깜빡이는 도중 게이지가 소모됐다면 어두워진 채로 멈추지 않도록 복구
		SetRenderOpacity(1.0f);
	}
}

const FFTSkillMetaData* UFTUltimateGaugeWidget::ResolveSkillRow() const
{
	//	소유 플레이어 캐릭터 → CharacterData(FFTCharacterData) → RSkill(궁극기) 스킬 행.
	const AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetOwningPlayerPawn());
	if (!Char)
	{
		return nullptr;
	}

	const FFTCharacterData* CharData = Char->GetCharacterData();
	if (!CharData || CharData->RSkill.IsNull())
	{
		return nullptr;
	}

	return CharData->RSkill.GetRow<FFTSkillMetaData>(TEXT("UFTUltimateGaugeWidget::ResolveSkillRow"));
}

void UFTUltimateGaugeWidget::RefreshSkillMetaData()
{
	//	소유 캐릭터의 CharacterData가 준비돼야 RSkill 행을 확정할 수 있다.
	//	아직이면(복제/스폰 지연) 재시도 타이머가 다시 호출한다.
	const AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetOwningPlayerPawn());
	if (!Char || !Char->GetCharacterData())
	{
		return;
	}

	//	데이터가 확보되면 이 위젯의 메타데이터 해석은 확정이다. RSkill이 비어 있어도(표시할 스킬 없음)
	//	정상 상태이므로, 여기서 재시도를 종료한다. (무한 재시도 방지)
	bSkillMetaResolved = true;

	const FFTSkillMetaData* Skill = ResolveSkillRow();
	if (!Skill)
	{
		return;
	}

	if (SkillName)
	{
		SkillName->SetText(Skill->DisplayName);
	}
	if (SkillDescription)
	{
		SkillDescription->SetText(Skill->Description);
	}
	if (SkillImage)
	{
		//	아이콘 텍스처는 소프트 참조 — 표시하는 이 시점에만 로드한다.
		if (UTexture2D* IconTexture = Skill->Icon.LoadSynchronous())
		{
			SkillImage->SetBrushFromTexture(IconTexture);
		}
	}
}
