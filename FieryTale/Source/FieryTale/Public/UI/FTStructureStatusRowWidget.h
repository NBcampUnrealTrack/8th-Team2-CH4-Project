// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FTStructureStatusRowWidget.generated.h"

class UImage;
class UTextBlock;
class AFTTowerBase;
class UMaterialInstanceDynamic;
struct FOnAttributeChangeData;


UCLASS()
class FIERYTALE_API UFTStructureStatusRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 표시할 타워/넥서스 액터를 지정해 바인딩을 시작한다.
	void InitializeWithStructure(AFTTowerBase* InStructure);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StructureNameText;

private:
	void BindToStructure(AFTTowerBase* InStructure);
	void UnbindFromStructure();
	
	void RefetchInitialAttributes();

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleVulnerabilityTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleStructureDestroyed();

	void RefreshHealthDisplay();
	void RefreshLockDisplay();

	UPROPERTY()
	TObjectPtr<AFTTowerBase> BoundStructure;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicFillMaterial;

	float CachedHealth = 0.f;
	float CachedMaxHealth = 0.f;
};
