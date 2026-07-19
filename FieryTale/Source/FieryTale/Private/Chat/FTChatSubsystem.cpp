// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/FTChatSubsystem.h"

#include "Chat/FTChatComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

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

void UFTChatSubsystem::PushSystemMessage(const FString& Text)
{
	// System 메시지는 발신자가 없고 네트워크로 나가지 않는다.
	// 수신/표시 경로(HandleMessageReceived)를 그대로 재사용해 기록에 쌓고 UI에 뿌린다.
	FFTChatMessage Message;
	Message.Message = Text;
	Message.Channel = EFTChatChannel::System;
	Message.Timestamp = (GetGameInstance() && GetGameInstance()->GetWorld())
		? GetGameInstance()->GetWorld()->GetTimeSeconds()
		: 0.0;

	HandleMessageReceived(Message);
}

void UFTChatSubsystem::HandleMessageReceived(const FFTChatMessage& Message)
{
	//	메시지를 채널별 버킷에 분리 보관한다.
	TArray<FFTChatMessage>& Bucket = GetMutableChannelHistory(Message.Channel);
	Bucket.Add(Message);

	// 오래된 기록부터 잘라 해당 채널의 상한을 유지한다.
	if (MaxHistory > 0 && Bucket.Num() > MaxHistory)
	{
		Bucket.RemoveAt(0, Bucket.Num() - MaxHistory);
	}

	OnMessageReceived.Broadcast(Message);
}

TArray<FFTChatMessage> UFTChatSubsystem::GetHistory() const
{
	//	세 채널 버킷을 시각순으로 병합한 통합 뷰(최근 MaxHistory개로 제한).
	//	모든 채널을 한 창에 표시하는 통합 채팅 위젯이 과거 기록을 복원할 때 쓴다.
	TArray<FFTChatMessage> Merged;
	Merged.Reserve(AllHistory.Num() + TeamHistory.Num() + SystemHistory.Num());
	Merged.Append(AllHistory);
	Merged.Append(TeamHistory);
	Merged.Append(SystemHistory);

	//	타임스탬프가 같은 메시지들은 버킷 내 원래 순서를 보존하도록 안정 정렬한다.
	Merged.StableSort([](const FFTChatMessage& A, const FFTChatMessage& B)
	{
		return A.Timestamp < B.Timestamp;
	});

	if (MaxHistory > 0 && Merged.Num() > MaxHistory)
	{
		Merged.RemoveAt(0, Merged.Num() - MaxHistory);
	}

	return Merged;
}

const TArray<FFTChatMessage>& UFTChatSubsystem::GetChannelHistory(EFTChatChannel Channel) const
{
	switch (Channel)
	{
	case EFTChatChannel::Team:   return TeamHistory;
	case EFTChatChannel::System: return SystemHistory;
	case EFTChatChannel::All:
	default:                     return AllHistory;
	}
}

TArray<FFTChatMessage>& UFTChatSubsystem::GetMutableChannelHistory(EFTChatChannel Channel)
{
	//	상수 버전과 동일 매핑을 재사용한다(Effective C++ 관용구: 비상수 → 상수 위임).
	return const_cast<TArray<FFTChatMessage>&>(GetChannelHistory(Channel));
}

void UFTChatSubsystem::ClearHistory()
{
	AllHistory.Reset();
	TeamHistory.Reset();
	SystemHistory.Reset();
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
