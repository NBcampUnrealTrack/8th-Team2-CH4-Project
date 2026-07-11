// Fill out your copyright notice in the Description page of Project Settings.


#include "MainMenu/FTMainMenu.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h" // PlayAnimation 함수를 위해 추가
#include "MainMenu/FTMainMenuWidget.h"

void UFTMainMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (PlayButton)
	{
		PlayButton->OnHovered.AddDynamic(this, &UFTMainMenu::OnPlayButtonHovered);
		PlayButton->OnUnhovered.AddDynamic(this, &UFTMainMenu::OnPlayButtonUnhovered);
		PlayButton->OnClicked.AddDynamic(this, &UFTMainMenu::OnPlayButtonClicked);
	}

	// 애니메이션 바인딩 확인 (개발 중 유용)
	if (!HoverAnimation)
	{
		UE_LOG(LogTemp, Error, TEXT("WBP_MainMenu에서 'HoverAnimation'을 찾을 수 없습니다."));
	}
	
	if (WBP_MainMenuWidget)
	{
		WBP_MainMenuWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UFTMainMenu::OnPlayButtonHovered()
{
	// 마우스를 올렸을 때 애니메이션을 정방향으로 재생합니다.
	// 이미 재생 중이라면 처음부터 다시 재생하지 않고 이어서 재생되도록 설정합니다.
	if (HoverAnimation)
	{
		// bNumLoopsToPlay를 1로 설정하여 한 번만 재생합니다.
		// EUMGSequencePlayMode::Forward로 정방향 재생합니다.
		PlayAnimation(HoverAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward);
	}
}

void UFTMainMenu::OnPlayButtonUnhovered()
{
	// 마우스가 벗어났을 때 애니메이션을 역방향으로 재생하여 원래 위치와 색상으로 복구시킵니다.
	if (HoverAnimation)
	{
		// EUMGSequencePlayMode::Reverse로 역방향 재생합니다.
		PlayAnimation(HoverAnimation, 0.0f, 1, EUMGSequencePlayMode::Reverse);
	}
}

void UFTMainMenu::OnPlayButtonClicked()
{
	// 클릭 시 실행할 로직
	UE_LOG(LogTemp, Warning, TEXT("플레이 버튼이 클릭되었습니다!"));
	
	if (WBP_MainMenuWidget)
	{
		WBP_MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}
	
	PlayButton->SetIsEnabled(false);
}