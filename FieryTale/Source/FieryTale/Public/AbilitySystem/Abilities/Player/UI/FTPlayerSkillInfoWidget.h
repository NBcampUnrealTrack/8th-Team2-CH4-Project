// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FTPlayerSkillInfoWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UAbilitySystemComponent;
class UFT_GameplayAbility;   // [구 경로] 어빌리티 CDO 조회용 — 주석과 함께 보존
class AFTPlayerCharacterBase;
struct FFTSkillMetaData;
struct FFTCharacterData;
struct FDataTableRowHandle;

/** 스킬 슬롯 — 캐릭터 데이터의 4개 버튼 슬롯 중 하나를 가리킨다. */
UENUM(BlueprintType)
enum class EFTSkillSlot : uint8
{
	LMB   UMETA(DisplayName = "LMB (좌클릭)"),
	RMB   UMETA(DisplayName = "RMB (우클릭)"),
	Space UMETA(DisplayName = "Space (이동/유틸)"),
	R     UMETA(DisplayName = "R (궁극기)")
};

/**
 *	스킬 슬롯 1개의 정보(아이콘/이름/설명)와 쿨타임을 표시하는 위젯.
 *	- SkillSlot으로 슬롯(LMB/RMB/Space/R)만 지정하면, 소유 플레이어 캐릭터의 CharacterData(FFTCharacterData)에서
 *	  해당 슬롯의 스킬 행(FFTSkillMetaData)과 쿨다운 태그를 가져와 아이콘/이름/설명/쿨타임을 표시한다. (DataTable 기반)
 *	- 미리 태그를 지정할 필요 없이, 슬롯만 정하면 캐릭터 데이터에서 스킬·태그를 복제해온다.
 *	- 표시용 위젯(Image/Text/ProgressBar)은 모두 BindWidgetOptional이라, 필요한 것만 WBP에 배치하면 된다.
 */
UCLASS()
class FIERYTALE_API UFTPlayerSkillInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//	HUD/컨트롤러가 ASC를 직접 알고 있을 때 명시적으로 초기화 (선택)
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	//	이 위젯이 표시할 스킬 슬롯(LMB/RMB/Space/R). 소유 플레이어 캐릭터의 CharacterData에서
	//	이 슬롯의 스킬 메타데이터/쿨다운 태그를 가져온다. (미리 태그를 지정할 필요 없음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Skill")
	EFTSkillSlot SkillSlot = EFTSkillSlot::LMB;

	//	아이콘 영역에 사용할 머티리얼(인스턴스). 아이콘 텍스처와 남은 쿨타임(0~1)을
	//	이 머티리얼의 파라미터로 전달해 쿨타임 스윕 등을 표시하는 용도. (동적 인스턴스 생성/파라미터 세팅은 소비 측에서)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Skill")
	TObjectPtr<UMaterialInterface> CooldownIconMaterial;

	//	머티리얼에 넘길 값을 한 번에 반환: 아이콘 텍스처(OutIcon) + 남은 쿨타임 비율(OutCooldownRatio, 0~1).
	//	현재 표시할 스킬이 있으면 true.
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Skill")
	bool GetSkillIconAndCooldownRatio(UTexture2D*& OutIcon, float& OutCooldownRatio) const;

	//	현재 남은 쿨타임 비율(0~1). 1=방금 시전, 0=사용 가능. (머티리얼 스칼라 파라미터 갱신용)
	UFUNCTION(BlueprintPure, Category = "FieryTale|Skill")
	float GetCooldownRatio() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//	스킬 아이콘 (머티리얼) — 스킬 행(FFTSkillMetaData)의 Icon으로 채운다
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
	//	소유 Pawn에서 ASC + 스킬 행을 찾아 바인딩. 아직이면(복제/부여 지연) 재시도 타이머를 건다.
	void TryBindToOwnerASC();
	UAbilitySystemComponent* ResolveOwnerASC() const;

	//	[구 경로/폐기 보존] SkillInputTag로 부여된 어빌리티(CDO)를 찾던 방식.
	//UFT_GameplayAbility* ResolveAbilityCDO() const;

	//	소유 캐릭터의 CharacterData(FFTCharacterData)에서 SkillSlot 슬롯의 스킬 행을 조회한다. 못 찾으면 nullptr.
	const FFTSkillMetaData* ResolveSkillRow() const;

	//	SkillSlot(버튼)에 대응하는 슬롯 핸들(LMBSkill/RMBSkill/SpaceSkill/RSkill)을 고른다.
	const FDataTableRowHandle* GetSlotHandle(const FFTCharacterData& CharData) const;

	//	스킬 행의 메타데이터로 아이콘/이름/설명을 채우고, 쿨다운 감시를 1회 등록한다.
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

	//	CooldownIconMaterial로부터 생성한 아이콘 영역용 동적 인스턴스.
	//	텍스처 파라미터 "Icon"(아이콘)과 스칼라 파라미터 "CooldownTime"(남은 쿨타임 0~1)을 갱신한다.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> IconMID;

	//	현재 감시 중인 쿨다운 태그 (대상 어빌리티의 CooldownTag). 어빌리티마다 다르다.
	FGameplayTag ActiveCooldownTag;

	//	ASC + 어빌리티 메타데이터까지 확보돼 표시가 끝났는지 (재시도 종료 판정용)
	bool bSkillInfoResolved = false;

	//	바인딩 재시도 횟수 및 상한 — ASC/캐릭터 데이터가 끝내 오지 않는 비정상 상황(예: 비플레이어 폰)에서
	//	무한 폴링을 막는 안전장치. 정상 구성에선 상한 도달 전에 해소된다.
	int32 BindRetryCount = 0;
	static constexpr int32 MaxBindRetries = 40; // 0.25s * 40 ≈ 10초

	FTimerHandle BindRetryTimer;
	FTimerHandle CooldownPollTimer;
	FDelegateHandle CooldownTagEventHandle;

	//	CacheCooldownWindow()가 채워주는 쿨다운 종료 시각(월드 시간)과 총 지속시간
	float CachedCooldownEndTime = 0.f;
	float CachedCooldownDuration = 0.f;
};
