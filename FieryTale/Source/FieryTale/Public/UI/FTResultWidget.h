// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTResultWidget.generated.h"

/**
 * 
 */
UCLASS()
class FIERYTALE_API UFTResultWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	// 블루프린트에서 만들 텍스트 블록과 이름이 정확히 일치해야 합니다.
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Text_MatchResult;

public:
	// 컨트롤러가 호출해 줄 함수 (승리 여부를 전달받음)
	void SetMatchResult(bool bIsVictory);
};
