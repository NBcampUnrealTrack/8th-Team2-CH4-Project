// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Player/UI/FTCircularProgressWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void UFTCircularProgressWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureGaugeMID();
}

void UFTCircularProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureGaugeMID();
}

void UFTCircularProgressWidget::EnsureGaugeMID()
{
	if (GaugeMID || !GaugeBar)
	{
		return;
	}

	GaugeMID = GaugeBar->GetDynamicMaterial();
}

void UFTCircularProgressWidget::SetPercent(float NewPercent)
{
	EnsureGaugeMID();
	if (GaugeMID)
	{
		GaugeMID->SetScalarParameterValue(TEXT("Percent"), FMath::Clamp(NewPercent, 0.f, 1.f));
	}
}

void UFTCircularProgressWidget::SetThickness(float NewThickness)
{
	EnsureGaugeMID();
	if (GaugeMID)
	{
		GaugeMID->SetScalarParameterValue(TEXT("Thickness"), NewThickness);
	}
}

void UFTCircularProgressWidget::SetCenterText(const FText& NewText)
{
	if (Second)
	{
		Second->SetText(NewText);
	}
}
