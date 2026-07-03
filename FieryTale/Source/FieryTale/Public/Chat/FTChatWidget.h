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

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_Message;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Send;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* Scroll_Messages;

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

private:
	UFUNCTION()
	void OnSendClicked();

	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleMessageReceived(const FFTChatMessage& Message);

	/** 입력창의 현재 내용을 전송하고 비운다. */
	void SubmitCurrentInput();

	/** 메시지 한 건을 스크롤 박스에 한 줄로 추가한다. (기반 단계: 단순 텍스트) */
	void AddMessageRow(const FFTChatMessage& Message);

	UPROPERTY()
	UFTChatSubsystem* ChatSubsystem;
};
