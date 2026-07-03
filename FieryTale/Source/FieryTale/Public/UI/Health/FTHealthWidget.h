#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FTHealthWidget.generated.h"

class UImage;
class UTextBlock;
class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
struct FOnAttributeChangeData;

UCLASS()
class FIERYTALE_API UFTHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ASC 연동 초기화
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

protected:
	// 위젯 파괴 시 타이머 정리
	virtual void NativeDestruct() override;

	// WBP의 UImage 컴포넌트 바인딩
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> HealthBar;

	// WBP의 UTextBlock 컴포넌트 바인딩
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

private:
	// ASC 데이터 구독 수립
	void BindToAttributes(UAbilitySystemComponent* InASC);

	// ASC 데이터 구독 해제
	void UnbindFromAttributes();

	// 실시간 최신 어트리뷰트 수치 동기화
	void FetchInitialAttributes();

	// 체력 변동 콜백
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	// 최대 체력 변동 콜백
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);

	// 사망 태그 이벤트 콜백
	void OnDeathTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 사망 지연 후 UI 숨김
	void HideWidgetAfterDelay();

	// 팩션 판별 및 틴트 컬러 적용
	void UpdateTeamColor();

	// 머티리얼 파라미터 동기화
	void RefreshHealthDisplay();

	// 바인딩된 ASC 참조 포인터
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	// 동적 머티리얼 캐싱 포인터
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicFillMaterial;

	// 로컬 체력 데이터 캐싱
	float CachedHealth = 100.f;

	// 로컬 최대 체력 데이터 캐싱
	float CachedMaxHealth = 100.f;

	// 유예 타이머 핸들
	FTimerHandle DeathTimerHandle;
};