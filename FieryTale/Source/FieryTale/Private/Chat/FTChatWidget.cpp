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
#include "Lobby/FTLobbyPlayerState.h"
#include "InputCoreTypes.h"

void UFTChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		ChatSubsystem = GameInstance->GetSubsystem<UFTChatSubsystem>();
		if (ChatSubsystem)
		{
			ChatSubsystem->OnMessageReceived.AddDynamic(this, &UFTChatWidget::HandleMessageReceived);
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

	//	표시 필터 버튼 바인딩(BP에 배치돼 있을 때만).
	if (Btn_ViewAll)
	{
		Btn_ViewAll->OnClicked.AddDynamic(this, &UFTChatWidget::OnViewAllClicked);
	}
	if (Btn_ViewTeam)
	{
		Btn_ViewTeam->OnClicked.AddDynamic(this, &UFTChatWidget::OnViewTeamClicked);
	}
	if (Btn_ViewSystem)
	{
		Btn_ViewSystem->OnClicked.AddDynamic(this, &UFTChatWidget::OnViewSystemClicked);
	}

	//	위젯이 늦게 생성됐어도(레벨 트래블 등) 현재 필터에 맞춰 기존 기록을 즉시 복원한다.
	RebuildMessageList();

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

FReply UFTChatWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	//	입력창에 포커스가 있는 동안 Tab → 전송 채널(전체 ↔ 팀) 토글.
	//	프리뷰 단계에서 처리하므로 EditableTextBox 의 기본 Tab 포커스 이동이 발생하기 전에 가로챈다.
	if (InKeyEvent.GetKey() == EKeys::Tab && Input_Message && Input_Message->HasKeyboardFocus())
	{
		ToggleSendChannel();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UFTChatWidget::SelectChannel(EFTChatChannel Channel)
{
	SendChannel = Channel;
	//	현재 전송 채널 표시를 BP에서 갱신할 수 있도록 알린다.
	OnSendChannelChanged(SendChannel);
}

void UFTChatWidget::ToggleSendChannel()
{
	//	전송 채널은 전체 ↔ 팀 만 순환한다. System 은 플레이어가 보내는 채널이 아니므로 제외한다.
	SelectChannel(SendChannel == EFTChatChannel::Team ? EFTChatChannel::All : EFTChatChannel::Team);
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
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	//	로비(대기방)에서는 화면 전체가 UI 모드이고 마우스로 슬롯/버튼을 조작해야 하므로,
	//	채팅 전송 후 게임 입력(GameOnly + 커서 숨김)으로 되돌리면 마우스가 사라진다.
	//	로비/아레나 구분은 코드 다른 곳(FTPlayerController::OnRep_PlayerState)과 동일하게
	//	PlayerState 타입으로 판별한다(로비=AFTLobbyPlayerState, 아레나=AFTPlayerState).
	if (PC->GetPlayerState<AFTLobbyPlayerState>() != nullptr)
	{
		//	입력창 포커스 잠금만 풀고 UI 모드·커서는 그대로 유지한다.
		FInputModeUIOnly UIMode;
		PC->SetInputMode(UIMode);
		PC->SetShowMouseCursor(true);
		return;
	}

	//	아레나(인게임)에서는 기존대로 게임 조작으로 복귀한다.
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(false);
}

void UFTChatWidget::HandleMessageReceived(const FFTChatMessage& Message)
{
	//	현재 표시 필터에 해당하는 채널의 메시지만 즉시 추가한다.
	if (PassesCurrentFilter(Message.Channel))
	{
		AddMessageRow(Message);
	}
}

void UFTChatWidget::OnViewAllClicked()
{
	SetFilterMode(EFTChatFilterMode::All);
}

void UFTChatWidget::OnViewTeamClicked()
{
	//	"팀 메세지" 탭 = 팀 채팅 + 시스템 알림.
	SetFilterMode(EFTChatFilterMode::TeamAndSystem);
}

void UFTChatWidget::OnViewSystemClicked()
{
	SetFilterMode(EFTChatFilterMode::SystemOnly);
}

void UFTChatWidget::SetFilterMode(EFTChatFilterMode NewMode)
{
	if (CurrentFilter == NewMode)
	{
		return;
	}
	CurrentFilter = NewMode;
	RebuildMessageList();
}

bool UFTChatWidget::PassesCurrentFilter(EFTChatChannel Channel) const
{
	switch (CurrentFilter)
	{
	case EFTChatFilterMode::SystemOnly:
		return Channel == EFTChatChannel::System;
	case EFTChatFilterMode::TeamAndSystem:
		return Channel == EFTChatChannel::Team || Channel == EFTChatChannel::System;
	case EFTChatFilterMode::All:
	default:
		return true;
	}
}

void UFTChatWidget::RebuildMessageList()
{
	if (!Scroll_Messages || !ChatSubsystem)
	{
		return;
	}

	//	기존 행을 모두 비우고 현재 필터에 맞는 채널 기록에서 다시 채운다.
	//	채널별 버킷에서 직접 가져오므로, 통합 뷰의 총량 상한 때문에 오래된 팀/시스템 기록이
	//	잘려나가는 일 없이 각 채널의 보관분을 온전히 복원한다.
	Scroll_Messages->ClearChildren();

	switch (CurrentFilter)
	{
	case EFTChatFilterMode::SystemOnly:
		for (const FFTChatMessage& Message : ChatSubsystem->GetChannelHistory(EFTChatChannel::System))
		{
			AddMessageRow(Message);
		}
		break;

	case EFTChatFilterMode::TeamAndSystem:
	{
		//	팀·시스템 두 버킷을 시각순으로 병합해 표시한다.
		TArray<FFTChatMessage> Merged = ChatSubsystem->GetChannelHistory(EFTChatChannel::Team);
		Merged.Append(ChatSubsystem->GetChannelHistory(EFTChatChannel::System));
		Merged.StableSort([](const FFTChatMessage& A, const FFTChatMessage& B)
		{
			return A.Timestamp < B.Timestamp;
		});
		for (const FFTChatMessage& Message : Merged)
		{
			AddMessageRow(Message);
		}
		break;
	}

	case EFTChatFilterMode::All:
	default:
		//	GetHistory()가 세 채널을 시각순으로 병합해 돌려준다.
		for (const FFTChatMessage& Message : ChatSubsystem->GetHistory())
		{
			AddMessageRow(Message);
		}
		break;
	}
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
