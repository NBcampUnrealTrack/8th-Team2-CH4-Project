// Fill out your copyright notice in the Description page of Project Settings.


#include "Chat/FTChatWidget.h"

#include "Chat/FTChatSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Character/FTPlayerController.h"

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

	//	컨트롤러가 엔터(ChatAction)로 이 입력창에 포커스를 줄 수 있도록 자신을 등록한다.
	//	HUD 안에 중첩돼 있어도 이 자가 등록으로 컨트롤러가 참조를 얻는다.
	if (AFTPlayerController* PC = Cast<AFTPlayerController>(GetOwningPlayer()))
	{
		PC->SetActiveChatWidget(this);
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
	//	버튼 전송도 엔터 전송과 동일하게 게임 조작으로 복귀시킨다.
	RestoreGameInput();
}

void UFTChatWidget::OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// 엔터로 확정했을 때만 전송한다. (포커스 이탈 등 다른 커밋은 무시)
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitCurrentInput();
		//	엔터 전송 후에는 곧바로 게임 조작으로 돌아간다.
		//	(내용이 비어 SubmitCurrentInput이 전송하지 않았더라도 채팅은 닫아 입력 잠금을 풀어준다.)
		RestoreGameInput();
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

	//	다음 입력을 위해 입력창을 비운다. (포커스/입력 모드 복귀는 호출부에서 RestoreGameInput으로 처리)
	Input_Message->SetText(FText::GetEmpty());
}

void UFTChatWidget::FocusMessageInput()
{
	if (!Input_Message)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	//	타이핑하는 동안 WASD 등 게임 입력이 캐릭터를 조작하지 않도록 UI 전용으로 잠근다.
	//	이 동안엔 컨트롤러의 ChatAction(엔터)도 발동하지 않으므로, 전송 엔터가 다시 열기로 이어지지 않는다.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Input_Message->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);

	Input_Message->SetKeyboardFocus();
}

void UFTChatWidget::RestoreGameInput()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
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
