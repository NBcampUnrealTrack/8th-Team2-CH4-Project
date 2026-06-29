// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/FTChatComponent.h"

#include "Chat/FTChatSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UFTChatComponent::UFTChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// PlayerController에 붙어 RPC를 주고받아야 하므로 복제 컴포넌트로 둔다.
	SetIsReplicatedByDefault(true);
}

void UFTChatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유 클라이언트(또는 리슨 서버 호스트)에서만 로컬 송신 통로로 서브시스템에 자신을 등록한다.
	// 서버에 존재하는 '원격 클라이언트들의 컴포넌트'는 로컬이 아니므로 등록하지 않는다.
	const APlayerController* PC = GetOwningPlayerController();
	if (PC && PC->IsLocalController())
	{
		if (UFTChatSubsystem* ChatSys = GetChatSubsystem())
		{
			ChatSys->RegisterLocalComponent(this);
		}
	}
}

void UFTChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFTChatSubsystem* ChatSys = GetChatSubsystem())
	{
		ChatSys->UnregisterLocalComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UFTChatComponent::SendMessage(const FString& Text, EFTChatChannel Channel)
{
	if (Text.IsEmpty())
	{
		return;
	}

	UE_LOG(LogFTChat, Log, TEXT("[Chat] 1) 송신 요청 → ServerRPC: 채널=%d, 길이=%d"),
		static_cast<int32>(Channel), Text.Len());

	Server_SendMessage(Text, Channel);
}

bool UFTChatComponent::Server_SendMessage_Validate(const FString& Text, EFTChatChannel Channel)
{
	// 빈 문자열/과도한 길이 1차 차단. (스팸·치트 방어의 시작점)
	//	빈 문자열을 클라이언트에서 보낼 수 없음
	return !Text.IsEmpty() && Text.Len() <= 512;
}

void UFTChatComponent::Server_SendMessage_Implementation(const FString& Text, EFTChatChannel Channel)
{
	FFTChatMessage Message;
	Message.SenderName = ResolveSenderName();
	Message.Message = Text;       // TODO: 서버측 욕설/스팸 필터, 트림 등 정제 단계
	Message.Channel = Channel;
	Message.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	UE_LOG(LogFTChat, Log, TEXT("[Chat] 2) 서버 수신: [%s] %s"), *Message.SenderName, *Message.Message);

	ServerDistributeMessage(Message);
}

void UFTChatComponent::ServerDistributeMessage(const FFTChatMessage& Message)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// GameMode/GameState에 의존하지 않고, 월드에 접속한 모든 PlayerController를 직접 순회한다.
	int32 Delivered = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		// TODO: Channel == Team 이면 발신자와 같은 팀의 컨트롤러에게만 전달하도록 필터 (팀 시스템 도입 시)

		if (UFTChatComponent* TargetComp = PC->FindComponentByClass<UFTChatComponent>())
		{
			TargetComp->Client_ReceiveMessage(Message);
			++Delivered;
		}
	}

	UE_LOG(LogFTChat, Log, TEXT("[Chat] 3) 분배 완료: %d명에게 전달"), Delivered);
}

void UFTChatComponent::Client_ReceiveMessage_Implementation(const FFTChatMessage& Message)
{
	UE_LOG(LogFTChat, Log, TEXT("[Chat] 4) 클라 수신: [%s] %s"), *Message.SenderName, *Message.Message);
	RouteToSubsystem(Message);
}

void UFTChatComponent::RouteToSubsystem(const FFTChatMessage& Message) const
{
	if (UFTChatSubsystem* ChatSys = GetChatSubsystem())
	{
		ChatSys->HandleMessageReceived(Message);
	}
}

APlayerController* UFTChatComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

FString UFTChatComponent::ResolveSenderName() const
{
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			const FString Name = PS->GetPlayerName();
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
	}
	return TEXT("Player");
}

UFTChatSubsystem* UFTChatComponent::GetChatSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UFTChatSubsystem>();
		}
	}
	return nullptr;
}
