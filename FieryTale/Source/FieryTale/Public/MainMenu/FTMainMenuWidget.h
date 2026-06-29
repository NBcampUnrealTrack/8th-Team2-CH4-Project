// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/FTSessionSubsystem.h"
#include "FTMainMenuWidget.generated.h"

/**
 * 
 */

class UEditableTextBox;
class UButton;
class UScrollBox;
class UTextBlock;

UCLASS()
class FIERYTALE_API UFTMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

	// --- 방 만들기 패널 ---
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_RoomName;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* Input_MaxPlayers;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_HostSession;

	// --- 방 찾기/참여 패널 ---
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_FindSessions;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* Scroll_SessionList;

	UPROPERTY(meta = (BindWidget))
	UButton* Btn_JoinSession;

	// --- 상태 표시 ---
	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_StatusMessage;

	// 리스트에 추가할 Row 위젯 클래스 (블루프린트에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UFTSessionRowWidget> SessionRowClass;

private:
	UFUNCTION()
	void OnHostButtonClicked();

	UFUNCTION()
	void OnFindButtonClicked();

	UFUNCTION()
	void OnJoinButtonClicked();

	// 서브시스템 델리게이트 콜백
	UFUNCTION()
	void HandleCreateSessionComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleFindSessionsComplete(const TArray<FFTSessionInfo>& SessionResults, bool bWasSuccessful);

	UFUNCTION()
	void HandleJoinSessionComplete(bool bWasSuccessful);

public:
	void SetSelectedSessionIndex(int32 Index);

private:
	UPROPERTY()
	UFTSessionSubsystem* SessionSubsystem;

	int32 SelectedSessionIndex = INDEX_NONE;	
	
};
