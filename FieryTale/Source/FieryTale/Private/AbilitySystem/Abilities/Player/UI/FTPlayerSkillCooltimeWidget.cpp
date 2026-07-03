// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/UI/FTPlayerSkillCooltimeWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/FTTags.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

UFTPlayerSkillCooltimeWidget::UFTPlayerSkillCooltimeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//	테스트 기본값(RMB 공격 스킬 쿨다운) — 필요 시 위젯 블루프린트 디테일 패널에서 슬롯별로 재지정
	//	실제로 현재 UtilSkill(Shift)만 Cooldown::UtilSkill을, RMB 스킬들은 Cooldown::RightClick을 사용함 (Cooldown::AttackSkill을 적용하는 어빌리티는 아직 없음)
	WatchedCooldownTag = FTTags::FTStates::Cooldown::RightClick;
	DisplaySkillName = FText::FromString(TEXT("A"));
}

void UFTPlayerSkillCooltimeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkillName)
	{
		SkillName->SetText(DisplaySkillName);
	}

	TryBindToOwnerASC();
}

void UFTPlayerSkillCooltimeWidget::NativeDestruct()
{
	UnbindFromASC();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	Super::NativeDestruct();
}

void UFTPlayerSkillCooltimeWidget::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC || InASC == BoundASC)
	{
		return;
	}

	UnbindFromASC();
	BindToASC(InASC);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}
}

void UFTPlayerSkillCooltimeWidget::TryBindToOwnerASC()
{
	if (BoundASC)
	{	//	이미 바인딩됨
		return;
	}

	if (UAbilitySystemComponent* ASC = ResolveOwnerASC())
	{
		BindToASC(ASC);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BindRetryTimer);
		}
		return;
	}

	//	ASC/PlayerState가 아직 준비되지 않음(특히 클라이언트 복제 지연) — 주기적으로 재시도
	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(BindRetryTimer))
		{
			World->GetTimerManager().SetTimer(
				BindRetryTimer, this, &UFTPlayerSkillCooltimeWidget::TryBindToOwnerASC, 0.25f, true);
		}
	}
}

UAbilitySystemComponent* UFTPlayerSkillCooltimeWidget::ResolveOwnerASC() const
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

void UFTPlayerSkillCooltimeWidget::BindToASC(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	BoundASC = InASC;

	if (!WatchedCooldownTag.IsValid())
	{
		UpdateCooldownDisplay();
		return;
	}

	CooldownTagEventHandle = BoundASC->RegisterGameplayTagEvent(WatchedCooldownTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UFTPlayerSkillCooltimeWidget::OnCooldownTagChanged);

	//	위젯이 늦게 생성/재바인딩될 수 있으므로(재접속 등) 현재 태그 상태로 즉시 동기화
	OnCooldownTagChanged(WatchedCooldownTag, BoundASC->GetTagCount(WatchedCooldownTag));
}

void UFTPlayerSkillCooltimeWidget::UnbindFromASC()
{
	StopCooldownPolling();

	if (BoundASC && WatchedCooldownTag.IsValid())
	{
		BoundASC->RegisterGameplayTagEvent(WatchedCooldownTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(CooldownTagEventHandle);
	}
	CooldownTagEventHandle.Reset();

	BoundASC = nullptr;
}

void UFTPlayerSkillCooltimeWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		//	쿨다운 시작 — 이 시점에 한 번만 ASC를 조회해 종료 시각을 캐싱하고, 그 이후엔 로컬 시간 계산만 폴링
		CacheCooldownWindow();
		StartCooldownPolling();
	}
	else
	{
		//	쿨다운 종료 — 폴링 중지하고 즉시 준비 완료 상태로 스냅
		StopCooldownPolling();
		if (CoolTimeBar)
		{
			CoolTimeBar->SetPercent(0.f);
		}
	}
}

void UFTPlayerSkillCooltimeWidget::CacheCooldownWindow()
{
	CachedCooldownEndTime = 0.f;
	CachedCooldownDuration = 0.f;

	UWorld* World = GetWorld();
	if (!BoundASC || !World || !WatchedCooldownTag.IsValid())
	{
		return;
	}

	const FGameplayTagContainer TagContainer(WatchedCooldownTag);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TagContainer);

	const TArray<float> Remaining = BoundASC->GetActiveEffectsTimeRemaining(Query);
	const TArray<float> Durations = BoundASC->GetActiveEffectsDuration(Query);

	if (Remaining.Num() > 0 && Durations.Num() > 0 && Durations[0] > 0.f)
	{
		CachedCooldownDuration = Durations[0];
		CachedCooldownEndTime = World->GetTimeSeconds() + Remaining[0];
	}
}

void UFTPlayerSkillCooltimeWidget::StartCooldownPolling()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(CooldownPollTimer))
	{
		return;
	}

	//	즉시 한 번 반영 후 주기적으로 갱신. ASC는 더 이상 건드리지 않고 캐싱된 종료 시각만 확인
	UpdateCooldownDisplay();

	World->GetTimerManager().SetTimer(
		CooldownPollTimer, this, &UFTPlayerSkillCooltimeWidget::UpdateCooldownDisplay, 1.0f, true);
}

void UFTPlayerSkillCooltimeWidget::StopCooldownPolling()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownPollTimer);
	}
}

void UFTPlayerSkillCooltimeWidget::UpdateCooldownDisplay()
{
	if (!CoolTimeBar)
	{
		return;
	}

	float Percent = 0.f;

	UWorld* World = GetWorld();
	if (World && CachedCooldownDuration > 0.f)
	{
		const float RemainingTime = CachedCooldownEndTime - World->GetTimeSeconds();
		Percent = FMath::Clamp(RemainingTime / CachedCooldownDuration, 0.f, 1.f);

		if (RemainingTime <= 0.f)
		{
			//	태그 제거 이벤트가 아직 안 왔더라도 계산상 이미 끝났으면 폴링을 미리 정지 (안전장치)
			StopCooldownPolling();
		}
	}

	CoolTimeBar->SetPercent(Percent);
}
