// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTPlayerStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 *	(임시 플레이 테스트용) 플레이어 체력바 + 체력 수치 표시 위젯 베이스.
 *	소유 Pawn의 ASC(UFT_AttributeSet)에 바인딩해 Health/MaxHealth 변화를 자동 반영한다.
 */
UCLASS()
class FIERYTALE_API UFTPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//	HUD/컨트롤러가 ASC를 직접 알고 있을 때 명시적으로 초기화 (선택)
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//	체력 비율(0~1) 표시
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	//	"120 / 150" 형태의 수치 표시 (없어도 컴파일되도록 Optional)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

private:
	//	소유 Pawn에서 ASC를 찾아 바인딩. 실패하면(복제 지연 등) 재시도 타이머를 건다.
	void TryBindToOwnerASC();
	UAbilitySystemComponent* ResolveOwnerASC() const;

	void BindToAttributes(UAbilitySystemComponent* InASC);
	void UnbindFromAttributes();

	//	어트리뷰트 변경 콜백
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);

	void RefreshHealthDisplay();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	float CachedHealth = 0.f;
	float CachedMaxHealth = 0.f;

	FTimerHandle BindRetryTimer;
};
