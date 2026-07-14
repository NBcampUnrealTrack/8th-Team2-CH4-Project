// Copyright Epic Games, Inc. All Rights Reserved.
#include "Level/FTTowerBase.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "Net/UnrealNetwork.h"

AFTTowerBase::AFTTowerBase()
{
	PrimaryActorTick.bCanEverTick = false; // 불필요한 틱 연산 차단
	bReplicates = true; // 1P와 2P 간 액터 동기화 허용

	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent")); // 데미지 판정 인스턴스 생성
	AbilitySystemComponent->SetIsReplicated(true); // 클라이언트-서버 간 어빌리티 복제 허용
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal); // 트래픽 최적화 모드 세팅

	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet")); // 체력 스탯 인스턴스 생성

	bIsDestroyed = false; // 파괴 상태 초기화
	bIsDying = false; // 중복 판정 플래그 초기화
}

UAbilitySystemComponent* AFTTowerBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent; // 인터페이스 주소 반환
}

void AFTTowerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); // 부모 복제 규칙 인계
	DOREPLIFETIME(AFTTowerBase, bIsDestroyed); // 엔진에 bIsDestroyed 변수의 네트워크 복제 추적 지시
}

void AFTTowerBase::BeginPlay()
{
	Super::BeginPlay(); // 기초 기동

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this); // GAS 정보 주체 바인딩
		
		FGameplayTag TeamTag = GetTeamTag(); // 자식 진영 데이터 패칭
		if (TeamTag.IsValid()) AbilitySystemComponent->AddLooseGameplayTag(TeamTag); // 피아 식별 태그 삽입
		
		FGameplayTag StructTag = GetStructureTag(); // 자식 구조물 데이터 패칭
		if (StructTag.IsValid()) AbilitySystemComponent->AddLooseGameplayTag(StructTag); // 객체 식별 태그 삽입
	}
}

void AFTTowerBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (HasAuthority() && Data.NewValue <= 0.0f && !bIsDying) // 보안을 위해 파괴 판정은 오직 1P 서버에서만 단독 검사
	{
		bIsDying = true; // 서버 단 판정 종료 확정
		bIsDestroyed = true; // 이 변수를 true로 바꾸면 언리얼 엔진이 2P에게 자동으로 OnRep_IsDestroyed를 발송함

		NotifyGameMode(); // 서버 권한 내부에서 게임 룰 통보 함수 호출
		OnRep_IsDestroyed(); // 1P 서버 자신도 연출을 봐야 하므로 수동으로 OnRep 함수 즉시 호출
	}
}

void AFTTowerBase::OnRep_IsDestroyed()
{
	PerformDestructionEffects(); // 1P와 2P 화면 양쪽에서 동시에 자식 클래스들의 시각적 연출 함수를 가동
	OnStructureDestroyed.Broadcast(); 
}