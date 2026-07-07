// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/FTCharacterTypes.h"
#include "FTCharacterSelectButton.generated.h"

class UButton;
class UImage;
class UTextBlock;
class AFTPlayerController;
struct FFTCharacterData;

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFTCharacterSelectButton : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 위젯 블루프린트의 컴포넌트 이름과 100% 일치해야 합니다.
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Character;

	UPROPERTY(meta = (BindWidget))
	UImage* Img_Portrait;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* Text_Name;

public:
	// 🌟 메인 로비 위젯이 이 버튼을 생성한 직후 데이터를 주입하는 함수
	void InitializeButton(const FFTCharacterData* InCharData, AFTPlayerController* InPC);

private:
	// 클릭했을 때 컨트롤러에 요청을 보내기 위해 저장해둘 변수들
	UPROPERTY()
	AFTPlayerController* LobbyPC;

	EFTCharacterType StoredCharacterType;

	// 🌟 버튼이 클릭되었을 때 실행될 델리게이트 함수
	UFUNCTION()
	void OnButtonClicked();
};
