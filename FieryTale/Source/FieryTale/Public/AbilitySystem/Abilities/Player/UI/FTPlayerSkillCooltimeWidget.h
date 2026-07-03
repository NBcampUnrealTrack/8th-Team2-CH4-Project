// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FTPlayerSkillCooltimeWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;

/**
 *	(임시 플레이 테스트용) 플레이어 스킬 쿨타임 표시 위젯.
 *	WatchedCooldownTag로 지정한 스킬의 쿨다운 GameplayEffect를 ASC에서 조회해
 *	남은 비율을 ProgressBar에, 스킬 이름을 Text에 표시한다.
 */
UCLASS()
class FIERYTALE_API UFTPlayerSkillCooltimeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFTPlayerSkillCooltimeWidget(const FObjectInitializer& ObjectInitializer);

	//	HUD/컨트롤러가 ASC를 직접 알고 있을 때 명시적으로 초기화 (선택)
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	//	이 위젯이 감시할 스킬의 쿨다운 태그. 슬롯별로 위젯 블루프린트 디테일에서 재지정 가능
	//	(예: FTTags::FTStates::Cooldown::AttackSkill / UtilSkill / RightClick)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Skill")
	FGameplayTag WatchedCooldownTag;

	//	(임시 테스트용) 스킬 이름 표시. 한 글자만 넣어도 무방
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Skill")
	FText DisplaySkillName;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//	남은 쿨타임 비율(0~1) 표시. 시전 직후 1, 사용 가능해지면 0
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CoolTimeBar;

	//	스킬 이름 표시 (없어도 컴파일되도록 Optional)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillName;

private:
	//	소유 Pawn에서 ASC를 찾아 바인딩. 실패하면(복제 지연 등) 재시도 타이머를 건다.
	void TryBindToOwnerASC();
	UAbilitySystemComponent* ResolveOwnerASC() const;

	void BindToASC(UAbilitySystemComponent* InASC);
	void UnbindFromASC();

	//	쿨다운 태그가 붙고 떨어지는 시점만 감시 — 태그가 붙어있는 동안만 폴링을 돌린다
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);

	//	쿨다운이 막 시작된 시점에 딱 한 번만 ASC를 조회해 종료 시각/총 지속시간을 캐싱
	void CacheCooldownWindow();

	//	쿨타임 폴링 (WatchedCooldownTag가 활성인 구간에서만 동작). ASC 재조회 없이 캐싱된 종료 시각으로 계산
	void StartCooldownPolling();
	void StopCooldownPolling();
	void UpdateCooldownDisplay();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	FTimerHandle BindRetryTimer;
	FTimerHandle CooldownPollTimer;
	FDelegateHandle CooldownTagEventHandle;

	//	CacheCooldownWindow()가 채워주는 쿨다운 종료 시각(월드 시간)과 총 지속시간
	float CachedCooldownEndTime = 0.f;
	float CachedCooldownDuration = 0.f;
};
