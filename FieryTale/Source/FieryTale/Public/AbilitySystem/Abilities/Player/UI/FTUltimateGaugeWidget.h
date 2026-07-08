// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTUltimateGaugeWidget.generated.h"

class UImage;
class UTextBlock;
class UAbilitySystemComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AFTPlayerCharacterBase;
struct FOnAttributeChangeData;
struct FFTSkillMetaData;

/**
 *	(임시 플레이 테스트용) 플레이어 궁극기 게이지 표시 위젯.
 *	소유 Pawn의 ASC(UFT_AttributeSet)에 바인딩해 UltimateGauge/MaxUltimateGauge 변화를
 *	비율(0~1)로 원형 프로그레스(Image + 다이나믹 머티리얼)에, %로 Text에 표시한다.
 */
UCLASS()
class FIERYTALE_API UFTUltimateGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//	HUD/컨트롤러가 ASC를 직접 알고 있을 때 명시적으로 초기화 (선택)
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//	궁극기 게이지 비율(0~1)을 원형으로 표시할 Image (M_RoundProgressbar 다이나믹 머티리얼 적용)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GaugeBar;

	//	"63%" 형태의 퍼센트 수치 표시 (없어도 컴파일되도록 Optional)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GaugeText;

	//	게이지가 100%일 때 GaugeText 대신 표시할 궁극기 스킬 아이콘 (없어도 컴파일되도록 Optional)
	//	CharacterData(FFTCharacterData)의 RSkill 메타데이터(FFTSkillMetaData)의 Icon으로 채운다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SkillImage;

	//	궁극기 스킬 이름 (RSkill 메타데이터의 DisplayName) (없어도 컴파일되도록 Optional)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillName;

	//	궁극기 스킬 설명 (RSkill 메타데이터의 Description) (없어도 컴파일되도록 Optional)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillDescription;

	//	GaugeBar에 적용할 원형 프로그레스 머티리얼 (BP 디폴트에서 M_RoundProgressbar 지정)
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI")
	TObjectPtr<UMaterialInterface> RoundProgressMaterial;

	//	(테스트용) 체크 시 실제 ASC 값 대신 TestPercent를 표시에 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|UI|Test")
	bool bUseTestPercent = false;

	//	(테스트용) bUseTestPercent가 true일 때 게이지에 표시할 비율(0~1). 디자이너/디테일 패널에서 수정하면 즉시 반영됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|UI|Test",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bUseTestPercent"))
	float TestPercent = 0.5f;

	//	게이지가 100%일 때 초당 깜빡이는 횟수
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI|Blink", meta = (ClampMin = "0.1"))
	float BlinkPulsesPerSecond = 2.0f;

	//	깜빡임 중 최소 불투명도(0=완전 투명까지 깜빡, 1=깜빡임 없음)
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|UI|Blink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlinkMinOpacity = 0.35f;

private:
	//	소유 Pawn에서 ASC를 찾아 바인딩. 실패하면(복제 지연 등) 재시도 타이머를 건다.
	void TryBindToOwnerASC();
	UAbilitySystemComponent* ResolveOwnerASC() const;

	void BindToASC(UAbilitySystemComponent* InASC);
	void UnbindFromASC();

	//	GaugeBar에 다이나믹 머티리얼이 아직 없으면 RoundProgressMaterial로부터 생성해 적용 (NativePreConstruct/NativeConstruct 공용)
	void EnsureGaugeMID();

	//	어트리뷰트 변경 콜백 — 값이 바뀔 때만 갱신(폴링 불필요)
	void HandleUltimateGaugeChanged(const FOnAttributeChangeData& Data);
	void HandleMaxUltimateGaugeChanged(const FOnAttributeChangeData& Data);

	void RefreshGaugeDisplay();

	//	소유 캐릭터의 CharacterData(FFTCharacterData)에서 RSkill(궁극기) 메타데이터 행을 조회한다. 못 찾으면 nullptr.
	const FFTSkillMetaData* ResolveSkillRow() const;

	//	RSkill 메타데이터로 SkillImage/SkillName/SkillDescription을 1회 채운다. 성공하면 bSkillMetaResolved를 true로 만든다.
	void RefreshSkillMetaData();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	//	NativeConstruct에서 RoundProgressMaterial로부터 생성해 GaugeBar에 적용하는 다이나믹 머티리얼 인스턴스
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GaugeMID;

	float CachedUltimateGauge = 0.f;
	float CachedMaxUltimateGauge = 0.f;

	//	게이지가 100%에 도달해 깜빡임 애니메이션을 재생 중인지 여부
	bool bIsGaugeFull = false;

	//	RSkill 메타데이터(아이콘/이름/설명)까지 확보돼 표시가 끝났는지 (재시도 종료 판정용)
	bool bSkillMetaResolved = false;

	FTimerHandle BindRetryTimer;
};
