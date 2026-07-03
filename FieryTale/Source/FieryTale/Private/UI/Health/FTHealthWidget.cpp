#include "UI/Health/FTHealthWidget.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameplayTags/FTTags.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

void UFTHealthWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeathTimerHandle); // 잔존 타이머 안전 해제
	}
	UnbindFromAttributes(); // 이벤트 바인딩 일괄 초기화
	Super::NativeDestruct();
}

void UFTHealthWidget::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC) return; // 주입된 ASC 널 포인터 방어
	
	if (InASC == BoundASC)
	{
		FetchInitialAttributes(); // 주소값이 같아도 에디터 조기 등록 오류를 분쇄하기 위해 스탯 강제 갱신 프로세스 작동[cite: 1]
		return;
	}
	
	UnbindFromAttributes(); // 이전 연결망 해제
	BindToAttributes(InASC); // 신규 연결망 수립
}

void UFTHealthWidget::BindToAttributes(UAbilitySystemComponent* InASC)
{
	if (!InASC) return; // ASC 유효성 검사
	BoundASC = InASC; // 대상 포인터 저장

	SetVisibility(ESlateVisibility::SelfHitTestInvisible); // UI 렌더링 활성화 및 힛테스트 무시

	if (HealthBar && !DynamicFillMaterial) // 동적 머티리얼 캐싱 조건 확인
	{
		if (HealthBar->GetBrush().GetResourceObject() != nullptr) // 머티리얼 에셋 누락 방어
		{
			DynamicFillMaterial = HealthBar->GetDynamicMaterial(); // 다이내믹 머티리얼 인스턴스 캐싱
		}
	}

	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).AddUObject(this, &UFTHealthWidget::HandleHealthChanged); // 체력 변경 이벤트 구독
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UFTHealthWidget::HandleMaxHealthChanged); // 최대 체력 변경 이벤트 구독
	BoundASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UFTHealthWidget::OnDeathTagChanged); // 사망 태그 이벤트 구독

	FetchInitialAttributes(); // 초기 데이터 로드 프로세스 실행
}

void UFTHealthWidget::UnbindFromAttributes()
{
	if (!BoundASC) return; // 바인딩 상태 검사
	
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetHealthAttribute()).RemoveAll(this); // 체력 이벤트 해제
	BoundASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMaxHealthAttribute()).RemoveAll(this); // 최대 체력 이벤트 해제
	BoundASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved).RemoveAll(this); // 사망 이벤트 해제
	
	BoundASC = nullptr; // 포인터 참조 해제
}

void UFTHealthWidget::FetchInitialAttributes()
{
	if (!BoundASC) return; // 데이터 소스 유효성 검사

	bool bFound = false;
	float InitialHealth = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetHealthAttribute(), bFound); // 최신 실시간 체력 수치 추출
	if (bFound) CachedHealth = InitialHealth;
	
	float InitialMaxHealth = BoundASC->GetGameplayAttributeValue(UFT_AttributeSet::GetMaxHealthAttribute(), bFound); // 최신 실시간 최대 체력 수치 추출
	if (bFound) CachedMaxHealth = InitialMaxHealth;

	UpdateTeamColor(); // 피아식별 색상 동기화
	RefreshHealthDisplay(); // 그래픽 파라미터 전송 및 리드로우
}

void UFTHealthWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue; // 체력 캐싱 변동분 반영
	RefreshHealthDisplay(); // UI 수치 리프레시
}

void UFTHealthWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue; // 최대 체력 캐싱 변동분 반영
	RefreshHealthDisplay(); // UI 수치 리프레시
}

void UFTHealthWidget::OnDeathTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0) // 사망 조건 충족 검사
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(DeathTimerHandle, this, &UFTHealthWidget::HideWidgetAfterDelay, 2.0f, false); // 사망 지연 타이머 2초 가동
		}
	}
}

void UFTHealthWidget::HideWidgetAfterDelay()
{
	SetVisibility(ESlateVisibility::Collapsed); // UI 가시성 완전 제거
}

void UFTHealthWidget::UpdateTeamColor()
{
	if (!BoundASC || !HealthBar) return; // 컴포넌트 유효성 검사

	FLinearColor FinalColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f); // 디폴트 적색 설정

	if (APlayerController* LocalPC = GetOwningPlayer())
	{
		if (UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(LocalPC->GetPawn()))
		{
			bool bIsAlly = false;
			if (LocalASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue) && BoundASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) bIsAlly = true; // 블루팀 동맹 판정
			else if (LocalASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red) && BoundASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)) bIsAlly = true; // 레드팀 동맹 판정

			if (bIsAlly) HealthBar->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.1f, 1.0f)); // 아군 녹색 적용
			else HealthBar->SetColorAndOpacity(FLinearColor(1.0f, 0.05f, 0.0f, 1.0f)); // 적군 적색 적용
			return;
		}
	}
	HealthBar->SetColorAndOpacity(FinalColor); // 최종 연산 색상 할당
}

void UFTHealthWidget::RefreshHealthDisplay()
{
	const float SafeMaxHealth = (CachedMaxHealth > 0.f) ? CachedMaxHealth : 100.f; // 분모 0 에러 차단 폴백 처리
	const float SafeHealth = (CachedMaxHealth > 0.f) ? CachedHealth : 100.f; // 데이터 결핍 대응 폴백 처리

	if (HealthBar && DynamicFillMaterial)
	{
		const float Percent = FMath::Clamp((SafeHealth / SafeMaxHealth), 0.f, 1.f); // 클리핑 스칼라 비율 연산
		DynamicFillMaterial->SetScalarParameterValue(TEXT("MaxHealth"), SafeMaxHealth); // 최대 스탯 데이터 셰이더 전송
		
		const float Show10Ticks = (SafeMaxHealth <= 1000.f) ? 1.0f : 0.0f; // 1000 이하 구간 동적 분기 연산
		DynamicFillMaterial->SetScalarParameterValue(TEXT("Show10Ticks"), Show10Ticks); // 10단위 제어 파라미터 전송
		
		const float Show100Ticks = 1.0f; // 100단위 상시 활성화 고정
		DynamicFillMaterial->SetScalarParameterValue(TEXT("Show100Ticks"), Show100Ticks); // 100단위 제어 파라미터 전송

		const float Show1000Ticks = (SafeMaxHealth > 1000.f) ? 1.0f : 0.0f; // 1000 초과 대형 스탯 분기 연산
		DynamicFillMaterial->SetScalarParameterValue(TEXT("Show1000Ticks"), Show1000Ticks); // 1000단위 제어 파라미터 전송
		
		DynamicFillMaterial->SetScalarParameterValue(TEXT("Percent"), Percent); // 투명도 마스크 연산용 비율 전송
	}

	if (HealthText)
	{
		const FString Display = FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(SafeHealth), FMath::RoundToInt(SafeMaxHealth)); // 디스플레이 문자열 포맷팅
		HealthText->SetText(FText::FromString(Display)); // 수치 텍스트 갱신
	}
}