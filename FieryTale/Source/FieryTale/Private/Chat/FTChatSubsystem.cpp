// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/FTChatSubsystem.h"

#include "Chat/FTChatComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogFTChat);

void UFTChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 특정 GameMode 클래스가 아니라 '전역' GameMode 이벤트에 바인딩한다.
	// → 어느 레벨/모드든 플레이어가 접속하면 서버가 그 컨트롤러에 채팅 컴포넌트를 자동 부착할 수 있다.
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UFTChatSubsystem::OnGameModePostLogin);

	UE_LOG(LogFTChat, Log, TEXT("[Chat] 서브시스템 초기화 - GameModePostLogin 전역 바인딩"));
}

void UFTChatSubsystem::Deinitialize()
{
	if (PostLoginHandle.IsValid())
	{
		FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
		PostLoginHandle.Reset();
	}

	Super::Deinitialize();
}

void UFTChatSubsystem::OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	// PostLogin은 서버 권한에서만 호출된다. 서버가 각 컨트롤러에 복제 채팅 컴포넌트를 1개 붙인다.
	// (붙은 컴포넌트는 소유 클라이언트로 복제되어, 그쪽에서 로컬 송신 통로로 등록된다.)
	if (!NewPlayer || !NewPlayer->HasAuthority())
	{
		return;
	}

	if (NewPlayer->FindComponentByClass<UFTChatComponent>())
	{
		// 이미 부착된 경우(BP에서 미리 넣어둔 경우 등)는 건너뛴다.
		return;
	}

	UFTChatComponent* ChatComp = NewObject<UFTChatComponent>(NewPlayer, UFTChatComponent::StaticClass(), TEXT("FTChatComponent"));
	if (ChatComp)
	{
		//	클라이언트로 복제
		ChatComp->SetIsReplicated(true);
		ChatComp->RegisterComponent();
		UE_LOG(LogFTChat, Log, TEXT("[Chat] 컨트롤러 %s 에 채팅 컴포넌트 부착"), *NewPlayer->GetName());
	}
}

void UFTChatSubsystem::SendMessage(const FString& Text, EFTChatChannel Channel)
{
	UFTChatComponent* Component = LocalChatComponent.Get();

	// 등록 타이밍을 놓쳤을 때를 대비한 폴백: 로컬 PlayerController에서 직접 찾는다.
	if (!Component)
	{
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (const APlayerController* PC = GameInstance->GetFirstLocalPlayerController())
			{
				Component = PC->FindComponentByClass<UFTChatComponent>();
			}
		}
	}

	if (Component)
	{
		Component->SendMessage(Text, Channel);
	}
	else
	{
		UE_LOG(LogFTChat, Warning, TEXT("[Chat] SendMessage 실패: 로컬 채팅 컴포넌트 없음 (아직 미부착/복제 대기 중)"));
	}
}

void UFTChatSubsystem::HandleMessageReceived(const FFTChatMessage& Message)
{
	History.Add(Message);

	// 오래된 기록부터 잘라 상한을 유지한다.
	if (MaxHistory > 0 && History.Num() > MaxHistory)
	{
		History.RemoveAt(0, History.Num() - MaxHistory);
	}

	OnMessageReceived.Broadcast(Message);
}

void UFTChatSubsystem::ClearHistory()
{
	History.Reset();
}

void UFTChatSubsystem::RegisterLocalComponent(UFTChatComponent* Component)
{
	LocalChatComponent = Component;
	UE_LOG(LogFTChat, Log, TEXT("[Chat] 로컬 채팅 컴포넌트 등록"));
}

void UFTChatSubsystem::UnregisterLocalComponent(UFTChatComponent* Component)
{
	if (LocalChatComponent.Get() == Component)
	{
		LocalChatComponent.Reset();
	}
}
