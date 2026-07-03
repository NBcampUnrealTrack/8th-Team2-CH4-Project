// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/FTChatWidget.h"

#include "Chat/FTChatSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
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
	FLinearColor RowColor;
	switch (Message.Channel)
	{
	case EFTChatChannel::System:
		Composed = FString::Printf(TEXT("[System] %s %s"), *Message.SenderName, *Message.Message);
		RowColor = SystemChannelColor;
		break;
	case EFTChatChannel::Team:
		Composed = FString::Printf(TEXT("[Team] %s: %s"), *Message.SenderName, *Message.Message);
		RowColor = TeamChannelColor;
		break;
	default:
		Composed = FString::Printf(TEXT("%s: %s"), *Message.SenderName, *Message.Message);
		RowColor = AllChannelColor;
		break;
	}

	Line->SetText(FText::FromString(Composed));
	// 채널별로 지정된 색상을 적용한다.
	Line->SetColorAndOpacity(FSlateColor(RowColor));
	// 채팅창 폭을 넘는 긴 메시지를 잘라내지 않고 다음 줄로 넘긴다.
	Line->SetAutoWrapText(true);
	
	UPanelSlot* AddedSlot = Scroll_Messages->AddChild(Line);
	if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(AddedSlot))
	{
		// 행이 스크롤박스 폭을 가득 채워야 자동 줄바꿈의 기준 폭이 생긴다.
		ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollSlot->SetPadding(FMargin(4.f, 2.f));
	}

	Scroll_Messages->ScrollToEnd();
}
