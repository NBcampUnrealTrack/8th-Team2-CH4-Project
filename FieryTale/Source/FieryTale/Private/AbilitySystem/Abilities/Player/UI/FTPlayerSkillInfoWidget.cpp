// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UI/FTPlayerSkillInfoWidget.h"

//#include "AbilitySystem/Abilities/FT_GameplayAbility.h"   // [구 경로] ResolveAbilityCDO용 — 주석 보존
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "AbilitySystem/Abilities/Player/Data/FTSkillMetaData.h"
#include "Character/FTPlayerCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectTypes.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	//	CooldownIconMaterial의 파라미터 이름 (텍스처: 아이콘, 스칼라: 남은 쿨타임 0~1)
	const FName ParamIcon(TEXT("Icon"));
	const FName ParamCooldownTime(TEXT("CooldownTime"));
}

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

	//	안전장치: 상한을 넘으면 무한 폴링을 멈춘다. (정상 구성에선 도달 전에 해소됨)
	if (++BindRetryCount >= MaxBindRetries)
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
	//	소유 플레이어 캐릭터 → CharacterData(FFTCharacterData) → SkillSlot 슬롯의 스킬 행.
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
	//	위젯에 지정된 슬롯(LMB/RMB/Space/R)에 대응하는 캐릭터 데이터의 스킬 핸들을 고른다.
	switch (SkillSlot)
	{
	case EFTSkillSlot::LMB:   return &CharData.LMBSkill;
	case EFTSkillSlot::RMB:   return &CharData.RMBSkill;
	case EFTSkillSlot::Space: return &CharData.SpaceSkill;
	case EFTSkillSlot::R:     return &CharData.RSkill;
	default:                  return nullptr;
	}
}

void UFTPlayerSkillInfoWidget::RefreshSkillInfo()
{
	//	소유 캐릭터의 CharacterData가 준비돼야 이 슬롯을 확정할 수 있다.
	//	아직이면(복제/스폰 지연) 재시도 타이머가 다시 호출한다.
	const AFTPlayerCharacterBase* Char = Cast<AFTPlayerCharacterBase>(GetOwningPlayerPawn());
	if (!Char || !Char->GetCharacterData())
	{
		return;
	}

	//	데이터가 확보되면 이 슬롯 해석은 확정이다. 슬롯이 비어 있어(표시할 스킬 없음)
	//	ResolveSkillRow()가 nullptr여도 정상 상태이므로, 여기서 재시도를 종료한다. (무한 재시도 방지)
	bSkillInfoResolved = true;

	const FFTSkillMetaData* Skill = ResolveSkillRow();
	if (!Skill)
	{
		//	이 슬롯엔 스킬이 없음 — 표시할 것 없음. (재시도는 이미 종료)
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
	if (SkillIcon)
	{
		//	아이콘 텍스처는 소프트 참조 — 표시하는 이 시점에만 로드한다.
		UTexture2D* IconTexture = Skill->Icon.LoadSynchronous();

		if (CooldownIconMaterial)
		{
			//	머티리얼 방식: 브러시 재질을 지정하고 MID를 만들어 아이콘("Icon")과 쿨타임("CooldownTime")을 파라미터로 전달한다.
			SkillIcon->SetBrushFromMaterial(CooldownIconMaterial);
			IconMID = SkillIcon->GetDynamicMaterial();
			if (IconMID)
			{
				if (IconTexture)
				{
					IconMID->SetTextureParameterValue(ParamIcon, IconTexture);
				}
				IconMID->SetScalarParameterValue(ParamCooldownTime, GetCooldownRatio());
			}
		}
		else if (IconTexture)
		{
			//	머티리얼 미지정 시 폴백: 텍스처를 브러시에 직접 표시.
			IconMID = nullptr;
			SkillIcon->SetBrushFromTexture(IconTexture);
		}
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
		if (IconMID)
		{
			IconMID->SetScalarParameterValue(ParamCooldownTime, 0.f);
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
	const float Ratio = GetCooldownRatio();

	//	남은 초(텍스트용) 계산 및 계산상 종료 시 폴링 조기 정지.
	float RemainingTime = 0.f;
	UWorld* World = GetWorld();
	if (World && CachedCooldownDuration > 0.f)
	{
		RemainingTime = CachedCooldownEndTime - World->GetTimeSeconds();
		if (RemainingTime <= 0.f)
		{
			//	태그 제거 이벤트가 아직 안 왔더라도 계산상 끝났으면 폴링을 미리 정지 (안전장치)
			StopCooldownPolling();
			RemainingTime = 0.f;
		}
	}

	if (CooldownBar)
	{
		CooldownBar->SetPercent(Ratio);
	}
	if (IconMID)
	{
		IconMID->SetScalarParameterValue(ParamCooldownTime, Ratio);
	}
	if (CooldownText)
	{
		//	남은 초를 올림해서 표시. 준비 완료면 비운다.
		CooldownText->SetText(RemainingTime > 0.f
			? FText::AsNumber(FMath::CeilToInt(RemainingTime))
			: FText::GetEmpty());
	}
}

float UFTPlayerSkillInfoWidget::GetCooldownRatio() const
{
	const UWorld* World = GetWorld();
	if (!World || CachedCooldownDuration <= 0.f)
	{
		return 0.f;
	}
	const float RemainingTime = CachedCooldownEndTime - World->GetTimeSeconds();
	return FMath::Clamp(RemainingTime / CachedCooldownDuration, 0.f, 1.f);
}

bool UFTPlayerSkillInfoWidget::GetSkillIconAndCooldownRatio(UTexture2D*& OutIcon, float& OutCooldownRatio) const
{
	OutIcon = nullptr;
	OutCooldownRatio = 0.f;

	const FFTSkillMetaData* Skill = ResolveSkillRow();
	if (!Skill)
	{
		return false;
	}

	//	아이콘 텍스처(소프트 참조 로드) + 현재 남은 쿨타임 비율(0~1)을 함께 돌려준다.
	OutIcon = Skill->Icon.LoadSynchronous();
	OutCooldownRatio = GetCooldownRatio();
	return true;
}
