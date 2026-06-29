// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/FTChatWidget.h"

#include "Chat/FTChatSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UFTChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		ChatSubsystem = GameInstance->GetSubsystem<UFTChatSubsystem>();
		if (ChatSubsystem)
		{
			ChatSubsystem->OnMessageReceived.AddDynamic(this, &UFTChatWidget::HandleMessageReceived);

			// 위젯이 늦게 생성됐어도(레벨 트래블 등) 기존 기록을 즉시 복원한다.
			for (const FFTChatMessage& Message : ChatSubsystem->GetHistory())
			{
				AddMessageRow(Message);
			}
		}
	}

	if (Btn_Send)
	{
		Btn_Send->OnClicked.AddDynamic(this, &UFTChatWidget::OnSendClicked);
	}
	if (Input_Message)
	{
		Input_Message->OnTextCommitted.AddDynamic(this, &UFTChatWidget::OnTextCommitted);
	}
}

void UFTChatWidget::NativeDestruct()
{
	if (ChatSubsystem)
	{
		ChatSubsystem->OnMessageReceived.RemoveDynamic(this, &UFTChatWidget::HandleMessageReceived);
	}

	Super::NativeDestruct();
}

void UFTChatWidget::SelectChannel(EFTChatChannel Channel)
{
	SendChannel = Channel;
}

void UFTChatWidget::OnSendClicked()
{
	SubmitCurrentInput();
}

void UFTChatWidget::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// 엔터로 확정했을 때만 전송한다. (포커스 이탈 등 다른 커밋은 무시)
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitCurrentInput();
	}
}

void UFTChatWidget::SubmitCurrentInput()
{
	if (!ChatSubsystem || !Input_Message)
	{
		return;
	}

	const FString Text = Input_Message->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	ChatSubsystem->SendMessage(Text, SendChannel);

	// 입력창을 비우고 포커스를 유지해 연속 입력을 편하게 한다.
	Input_Message->SetText(FText::GetEmpty());
	Input_Message->SetKeyboardFocus();
}

void UFTChatWidget::HandleMessageReceived(const FFTChatMessage& Message)
{
	AddMessageRow(Message);
}

void UFTChatWidget::AddMessageRow(const FFTChatMessage& Message)
{
	if (!Scroll_Messages)
	{
		return;
	}

	// 기반 단계: 단순 한 줄 텍스트로 추가한다.
	// 더 화려한 Row(아바타/색상/타임스탬프 등)가 필요하면 별도 Row 위젯으로 교체하면 된다.
	UTextBlock* Line = NewObject<UTextBlock>(this);
	if (!Line)
	{
		return;
	}

	FString Composed;
	switch (Message.Channel)
	{
	case EFTChatChannel::System:
		Composed = FString::Printf(TEXT("[System] %s %s"),*Message.SenderName, *Message.Message);
		break;
	case EFTChatChannel::Team:
		Composed = FString::Printf(TEXT("[Team] %s: %s"), *Message.SenderName, *Message.Message);
		break;
	default:
		Composed = FString::Printf(TEXT("%s: %s"), *Message.SenderName, *Message.Message);
		break;
	}

	Line->SetText(FText::FromString(Composed));
	Scroll_Messages->AddChild(Line);
	Scroll_Messages->ScrollToEnd();
}
