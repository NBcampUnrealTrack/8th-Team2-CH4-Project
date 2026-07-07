// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FTPlayerSkillInfoWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class UAbilitySystemComponent;
class UFT_GameplayAbility;

/**
 *	스킬 슬롯 1개의 정보(아이콘/이름/설명)와 쿨타임을 표시하는 위젯.
 *	- SkillInputTag로 슬롯(RMB/Shift/Q 등)을 지정하면, 소유 플레이어의 ASC에서
 *	  그 인풋 태그로 부여된 어빌리티를 찾아 CDO(UFT_GameplayAbility)의 메타데이터를 읽어 표시한다.
 *	- 해당 어빌리티의 CooldownTag를 감시해 남은 쿨타임 비율을 CooldownBar에 표시한다.
 *	- 표시용 위젯(Image/Text/ProgressBar)은 모두 BindWidgetOptional이라, 필요한 것만 WBP에 배치하면 된다.
 */
UCLASS()
class FIERYTALE_API UFTPlayerSkillInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//	HUD/컨트롤러가 ASC를 직접 알고 있을 때 명시적으로 초기화 (선택)
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	//	이 위젯이 표시할 스킬 슬롯의 인풋 태그.
	//	(예: FTTags::FTAbilities::AttackSkill / UtilSkill / UltimateSkill / NormalAttack)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Skill")
	FGameplayTag SkillInputTag;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//	스킬 아이콘 (머티리얼) — 어빌리티 CDO의 SkillIcon으로 채운다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SkillIcon;

	//	스킬 이름
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillName;

	//	스킬 설명
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkillDescription;

	//	남은 쿨타임 비율(0~1). 시전 직후 1, 사용 가능해지면 0
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CooldownBar;

	//	남은 쿨타임 초 텍스트 (쿨다운 중이 아닐 땐 비운다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CooldownText;

private:
	//	소유 Pawn에서 ASC + 대상 어빌리티를 찾아 바인딩. 아직이면(복제/부여 지연) 재시도 타이머를 건다.
	void TryBindToOwnerASC();
	UAbilitySystemComponent* ResolveOwnerASC() const;

	//	SkillInputTag로 부여된 어빌리티(CDO)를 찾는다. 못 찾으면 nullptr.
	UFT_GameplayAbility* ResolveAbilityCDO() const;

	//	어빌리티 CDO의 메타데이터로 아이콘/이름/설명을 채우고, 쿨다운 감시를 1회 등록한다.
	//	성공하면 bSkillInfoResolved를 true로 만든다.
	void RefreshSkillInfo();

	void UnbindFromASC();

	//	쿨다운 태그가 붙고 떨어지는 시점만 감시 — 붙어있는 동안만 폴링을 돌린다
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);

	//	쿨다운 시작 시 딱 한 번 ASC를 조회해 종료 시각/총 지속시간을 캐싱
	void CacheCooldownWindow();

	void StartCooldownPolling();
	void StopCooldownPolling();
	void UpdateCooldownDisplay();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	//	현재 감시 중인 쿨다운 태그 (대상 어빌리티의 CooldownTag). 어빌리티마다 다르다.
	FGameplayTag ActiveCooldownTag;

	//	ASC + 어빌리티 메타데이터까지 확보돼 표시가 끝났는지 (재시도 종료 판정용)
	bool bSkillInfoResolved = false;

	FTimerHandle BindRetryTimer;
	FTimerHandle CooldownPollTimer;
	FDelegateHandle CooldownTagEventHandle;

	//	CacheCooldownWindow()가 채워주는 쿨다운 종료 시각(월드 시간)과 총 지속시간
	float CachedCooldownEndTime = 0.f;
	float CachedCooldownDuration = 0.f;
};
