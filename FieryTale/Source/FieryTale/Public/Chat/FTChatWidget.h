// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chat/FTChatTypes.h"
#include "FTChatWidget.generated.h"

class UEditableTextBox;
class UButton;
class UScrollBox;
class UFTChatSubsystem;

/**
 * 채팅창 표시 필터. 전송 채널(SendChannel)과는 별개로 "무엇을 보여줄지"를 결정한다.
 *  - All           : 전체(All+Team+System) 다 표시
 *  - TeamAndSystem : 팀 메시지 + 시스템 메시지
 *  - SystemOnly    : 시스템 메시지만
 */
UENUM(BlueprintType)
enum class EFTChatFilterMode : uint8
{
	All            UMETA(DisplayName = "전체 (All+Team+System)"),
	TeamAndSystem  UMETA(DisplayName = "팀+시스템 (Team+System)"),
	SystemOnly     UMETA(DisplayName = "시스템만 (System)")
};

/**
 * 채팅 UI 베이스 위젯. UFTChatSubsystem 하나에만 의존하므로 어떤 레벨/게임모드에서든
 * 그대로 띄울 수 있다. BP에서 이 클래스를 상속해 레이아웃만 구성하면 된다.
 *
 * 필요한 BindWidget 요소:
 *  - Input_Message  (UEditableTextBox) : 입력창
 *  - Btn_Send       (UButton)          : 전송 버튼
 *  - Scroll_Messages(UScrollBox)       : 메시지 목록
 */
UCLASS()
class FIERYTALE_API UFTChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 입력창에 키보드 포커스를 주고, 타이핑 중 게임 입력이 먹지 않도록 UIOnly로 전환한다.
	 *  게임 플레이 중 컨트롤러의 ChatAction(엔터)에서 호출한다.
	 *  위젯에서 직접 엔터를 감지할 수 없는 이유: 플레이 중 키보드 포커스는 게임 뷰포트에 있고
	 *  이 위젯엔 없기 때문이다. 그래서 열기 트리거는 반드시 컨트롤러 입력을 거쳐야 한다. */
	void FocusMessageInput();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//	입력창에 포커스가 있는 동안 Tab 을 가로채 전송 채널을 토글한다.
	//	프리뷰(터널) 단계라 EditableTextBox 의 기본 Tab 포커스 이동보다 먼저 처리된다.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_Message;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Send;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* Scroll_Messages;

	/** 표시 필터 버튼(선택적). BP에 배치하고 이름을 맞추면 자동 바인딩된다.
	 *  없어도 위젯은 정상 동작하며 기본 필터(전체)로 표시한다.
	 *   - Btn_ViewAll    : 전체 메시지
	 *   - Btn_ViewTeam   : 팀 메시지 + 시스템 메시지
	 *   - Btn_ViewSystem : 시스템 메시지만 */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_ViewAll;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_ViewTeam;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Btn_ViewSystem;

	/** 이 위젯이 보낼 채널. BP에서 채널 탭을 만들면 바꿔 끼운다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Chat")
	EFTChatChannel SendChannel = EFTChatChannel::All;

	/** All(전체) 채널 메시지 텍스트 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Chat|Color")
	FLinearColor AllChannelColor = FLinearColor::White;

	/** Team(팀) 채널 메시지 텍스트 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Chat|Color")
	FLinearColor TeamChannelColor = FLinearColor(0.35f, 0.7f, 1.0f, 1.0f);

	/** System(시스템) 채널 메시지 텍스트 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Chat|Color")
	FLinearColor SystemChannelColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);
	
	//	채팅 UI 탭에서 메세지를 보낼 채널을 선택할 경우 호출
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Chat")
	void SelectChannel(EFTChatChannel Channel = EFTChatChannel::All);

	/** 전송 채널이 바뀔 때(Tab 토글 / SelectChannel) 호출된다.
	 *  BP에서 현재 전송 채널 표시(라벨/플레이스홀더 등)를 갱신하는 데 쓴다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FieryTale|Chat")
	void OnSendChannelChanged(EFTChatChannel NewChannel);

private:
	UFUNCTION()
	void OnSendClicked();

	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** 전송 채널을 전체 ↔ 팀 으로만 토글한다(System 은 플레이어 전송 채널이 아님). */
	void ToggleSendChannel();

	UFUNCTION()
	void HandleMessageReceived(const FFTChatMessage& Message);

	/** 입력창의 현재 내용을 전송하고 비운다. */
	void SubmitCurrentInput();

	/** UIOnly로 잠갔던 입력 모드를 게임 전용으로 되돌린다(엔터 전송/버튼 전송 후 호출). */
	void RestoreGameInput();

	/** 메시지 한 건을 스크롤 박스에 한 줄로 추가한다. (기반 단계: 단순 텍스트) */
	void AddMessageRow(const FFTChatMessage& Message);

	//	--- 표시 필터 ---

	UFUNCTION()
	void OnViewAllClicked();

	UFUNCTION()
	void OnViewTeamClicked();

	UFUNCTION()
	void OnViewSystemClicked();

	/** 필터를 바꾸고(같으면 무시) 목록을 다시 그린다. */
	void SetFilterMode(EFTChatFilterMode NewMode);

	/** 현재 필터에 맞춰 스크롤 목록을 비우고 채널 기록에서 다시 채운다. */
	void RebuildMessageList();

	/** 라이브 수신 메시지가 현재 필터의 표시 대상인지 판정한다. */
	bool PassesCurrentFilter(EFTChatChannel Channel) const;

	/** 현재 표시 필터. 기본은 전체. */
	EFTChatFilterMode CurrentFilter = EFTChatFilterMode::All;

	UPROPERTY()
	UFTChatSubsystem* ChatSubsystem;
};
