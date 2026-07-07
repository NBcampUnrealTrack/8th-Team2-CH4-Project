// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UI/FTPlayerSkillInfoWidget.h"

//#include "AbilitySystem/Abilities/FT_GameplayAbility.h"   // [구 경로] ResolveAbilityCDO용 — 주석 보존
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "AbilitySystem/Abilities/Player/Data/FTSkillMetaData.h"
#include "Character/FTPlayerCharacterBase.h"
#include "GameplayTags/FTTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectTypes.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UFTPlayerSkillInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBindToOwnerASC();
}

void UFTPlayerSkillInfoWidget::NativeDestruct()
{
	UnbindFromASC();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	Super::NativeDestruct();
}

void UFTPlayerSkillInfoWidget::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	//	다른 ASC로 교체되는 경우 기존 바인딩부터 정리한다.
	if (InASC != BoundASC)
	{
		UnbindFromASC();
	}

	BoundASC = InASC;
	RefreshSkillInfo();

	//	ASC와 스킬 정보까지 확보됐으면 재시도 타이머 종료
	if (bSkillInfoResolved)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}
	}
}

void UFTPlayerSkillInfoWidget::TryBindToOwnerASC()
{
	//	1) ASC 확보 (플레이어/PlayerState 복제 지연 대비)
	if (!BoundASC)
	{
		if (UAbilitySystemComponent* ASC = ResolveOwnerASC())
		{
			BoundASC = ASC;
		}
	}

	//	2) ASC가 있으면 대상 어빌리티 메타데이터 확보 시도 (어빌리티 부여 지연 대비)
	if (BoundASC && !bSkillInfoResolved)
	{
		RefreshSkillInfo();
	}

	//	3) 둘 다 끝났으면 타이머 종료, 아니면 주기적으로 재시도
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (BoundASC && bSkillInfoResolved)
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(BindRetryTimer))
	{
		World->GetTimerManager().SetTimer(
			BindRetryTimer, this, &UFTPlayerSkillInfoWidget::TryBindToOwnerASC, 0.25f, true);
	}
}

UAbilitySystemComponent* UFTPlayerSkillInfoWidget::ResolveOwnerASC() const
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

/*	[구 경로/폐기 보존] SkillInputTag로 부여된 어빌리티 CDO를 찾아 CDO 메타데이터를 읽던 방식.
	이제 메타데이터는 DataTable(FFTSkillMetaData)에서 조회한다. (아래 ResolveSkillRow 참고)
UFT_GameplayAbility* UFTPlayerSkillInfoWidget::ResolveAbilityCDO() const
{
	if (!BoundASC || !SkillInputTag.IsValid())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : BoundASC->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		const bool bMatch =
			Spec.Ability->GetAssetTags().HasTagExact(SkillInputTag) ||
			Spec.GetDynamicSpecSourceTags().HasTagExact(SkillInputTag);

		if (bMatch)
		{
			return Cast<UFT_GameplayAbility>(Spec.Ability);
		}
	}

	return nullptr;
}
*/

const FFTSkillMetaData* UFTPlayerSkillInfoWidget::ResolveSkillRow() const
{
	//	소유 플레이어 캐릭터 → CharacterData(FFTCharacterData) → SkillInputTag 슬롯의 스킬 행.
	const AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetOwningPlayerPawn());
	if (!Char)
	{
		return nullptr;
	}

	const FFTCharacterData* CharData = Char->GetCharacterData();
	if (!CharData)
	{
		return nullptr;
	}

	const FDataTableRowHandle* SlotHandle = GetSlotHandle(*CharData);
	if (!SlotHandle || SlotHandle->IsNull())
	{
		return nullptr;
	}

	return SlotHandle->GetRow<FFTSkillMetaData>(TEXT("UFTPlayerSkillInfoWidget::ResolveSkillRow"));
}

const FDataTableRowHandle* UFTPlayerSkillInfoWidget::GetSlotHandle(const FFTCharacterData& CharData) const
{
	//	버튼→슬롯 매핑은 기존 인풋 태그를 재사용한다. (캐릭터 부여 로직과 동일한 대응)
	if (SkillInputTag == FTTags::FTAbilities::NormalAttack)  { return &CharData.LMBSkill; }
	if (SkillInputTag == FTTags::FTAbilities::AttackSkill)   { return &CharData.RMBSkill; }
	if (SkillInputTag == FTTags::FTAbilities::UtilSkill)     { return &CharData.SpaceSkill; }
	if (SkillInputTag == FTTags::FTAbilities::UltimateSkill) { return &CharData.RSkill; }
	return nullptr;
}

void UFTPlayerSkillInfoWidget::RefreshSkillInfo()
{
	const FFTSkillMetaData* Skill = ResolveSkillRow();
	if (!Skill)
	{
		//	아직 캐릭터/행이 준비되지 않았을 수 있다 — 재시도 타이머가 다시 호출한다.
		return;
	}

	//	메타데이터 표시 (DataTable 기반)
	if (SkillName)
	{
		SkillName->SetText(Skill->DisplayName);
	}
	if (SkillDescription)
	{
		SkillDescription->SetText(Skill->Description);
	}
	if (SkillIcon && Skill->Icon)
	{
		SkillIcon->SetBrushFromMaterial(Skill->Icon);
	}

	//	쿨다운 감시 1회 등록 — 행에 유효한 쿨다운 태그가 있고, ASC가 준비됐고, 아직 등록 전일 때만.
	if (Skill->CooldownTag.IsValid() && !ActiveCooldownTag.IsValid() && BoundASC)
	{
		ActiveCooldownTag = Skill->CooldownTag;
		CooldownTagEventHandle = BoundASC->RegisterGameplayTagEvent(ActiveCooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UFTPlayerSkillInfoWidget::OnCooldownTagChanged);

		//	이미 쿨다운 중일 수 있으므로 현재 태그 상태로 즉시 동기화
		OnCooldownTagChanged(ActiveCooldownTag, BoundASC->GetTagCount(ActiveCooldownTag));
	}

	bSkillInfoResolved = true;
}

void UFTPlayerSkillInfoWidget::UnbindFromASC()
{
	StopCooldownPolling();

	if (BoundASC && ActiveCooldownTag.IsValid())
	{
		BoundASC->RegisterGameplayTagEvent(ActiveCooldownTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(CooldownTagEventHandle);
	}
	CooldownTagEventHandle.Reset();

	ActiveCooldownTag = FGameplayTag();
	bSkillInfoResolved = false;
	BoundASC = nullptr;
}

void UFTPlayerSkillInfoWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		//	쿨다운 시작 — 이 시점에 한 번만 ASC를 조회해 종료 시각을 캐싱하고, 이후엔 로컬 시간 계산만 폴링
		CacheCooldownWindow();
		StartCooldownPolling();
	}
	else
	{
		//	쿨다운 종료 — 폴링 중지하고 즉시 준비 완료 상태로 스냅
		StopCooldownPolling();
		if (CooldownBar)
		{
			CooldownBar->SetPercent(0.f);
		}
		if (CooldownText)
		{
			CooldownText->SetText(FText::GetEmpty());
		}
	}
}

void UFTPlayerSkillInfoWidget::CacheCooldownWindow()
{
	CachedCooldownEndTime = 0.f;
	CachedCooldownDuration = 0.f;

	UWorld* World = GetWorld();
	if (!BoundASC || !World || !ActiveCooldownTag.IsValid())
	{
		return;
	}

	const FGameplayTagContainer TagContainer(ActiveCooldownTag);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TagContainer);

	const TArray<float> Remaining = BoundASC->GetActiveEffectsTimeRemaining(Query);
	const TArray<float> Durations = BoundASC->GetActiveEffectsDuration(Query);

	if (Remaining.Num() > 0 && Durations.Num() > 0 && Durations[0] > 0.f)
	{
		CachedCooldownDuration = Durations[0];
		CachedCooldownEndTime = World->GetTimeSeconds() + Remaining[0];
	}
}

void UFTPlayerSkillInfoWidget::StartCooldownPolling()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(CooldownPollTimer))
	{
		return;
	}

	//	즉시 한 번 반영 후 주기적으로 갱신. ASC는 더 이상 건드리지 않고 캐싱된 종료 시각만 확인.
	UpdateCooldownDisplay();

	World->GetTimerManager().SetTimer(
		CooldownPollTimer, this, &UFTPlayerSkillInfoWidget::UpdateCooldownDisplay, 0.1f, true);
}

void UFTPlayerSkillInfoWidget::StopCooldownPolling()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownPollTimer);
	}
}

void UFTPlayerSkillInfoWidget::UpdateCooldownDisplay()
{
	float Percent = 0.f;
	float RemainingTime = 0.f;

	UWorld* World = GetWorld();
	if (World && CachedCooldownDuration > 0.f)
	{
		RemainingTime = CachedCooldownEndTime - World->GetTimeSeconds();
		Percent = FMath::Clamp(RemainingTime / CachedCooldownDuration, 0.f, 1.f);

		if (RemainingTime <= 0.f)
		{
			//	태그 제거 이벤트가 아직 안 왔더라도 계산상 끝났으면 폴링을 미리 정지 (안전장치)
			StopCooldownPolling();
			RemainingTime = 0.f;
			Percent = 0.f;
		}
	}

	if (CooldownBar)
	{
		CooldownBar->SetPercent(Percent);
	}
	if (CooldownText)
	{
		//	남은 초를 올림해서 표시. 준비 완료면 비운다.
		CooldownText->SetText(RemainingTime > 0.f
			? FText::AsNumber(FMath::CeilToInt(RemainingTime))
			: FText::GetEmpty());
	}
}
