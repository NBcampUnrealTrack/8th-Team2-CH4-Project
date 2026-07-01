// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/FTPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"

AFTPlayerState::AFTPlayerState()
{
	// 멀티플레이어 환경에서 태그 및 상태 복제 패킷 주기를 최적화합니다.
	NetUpdateFrequency = 100.0f;

	// 1. 생성자 단계에서 프로젝트 커스텀 ASC를 안전하게 생성하고 복제(Replication)를 활성화합니다.
	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// FPS/TPS 슈터 멀티플레이 환경에서 가장 연산 및 데이터 동기화 효율이 좋은 Mixed 모드로 설정합니다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 2. 캐릭터가 바뀌어도 유지될 플레이어 코어 속성 데이터 셋(AttributeSet)을 생성합니다.
	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AFTPlayerState::GetAbilitySystemComponent() const
{
	// 레퍼런스 규격(ACC_PlayerState)에 맞춰 불필요한 Cast를 걷어내고 컴포넌트를 깔끔하게 다이렉트로 리턴합니다.
	return AbilitySystemComponent;
}

void AFTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFTPlayerState, SpawnedCharacter);
}



