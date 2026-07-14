// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FTAllyRosterRowWidget.generated.h"

class UImage;
class UTextBlock;
class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class AFTPlayerState;
struct FOnAttributeChangeData;

// 아군 로스터 패널의 한 줄 — 초상화 + 체력바, 사망 시 체력바 대신 Dead 텍스트로 전환된다.
// 체력바는 FTStructureStatusRowWidget과 동일하게 다이나믹 머티리얼(M_HealthBarFill) 기반 Image다.
UCLASS()
class FIERYTALE_API UFTAllyRosterRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithPlayerState(AFTPlayerState* InPlayerState);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HealthBarBackdrop;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DeadText;

private:
	void BindToAttributes(UAbilitySystemComponent* InASC);
	void UnbindFromAttributes();
	void FetchInitialAttributes();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void OnDeathTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RefreshHealthDisplay();

	// SpawnedCharacter가 아직 스폰 전이면 잠시 후 재시도한다.
	void TryResolvePortrait();

	UPROPERTY()
	TObjectPtr<AFTPlayerState> BoundPlayerState;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicFillMaterial;

	float CachedHealth = 100.f;
	float CachedMaxHealth = 100.f;

	FTimerHandle PortraitRetryTimerHandle;
};
