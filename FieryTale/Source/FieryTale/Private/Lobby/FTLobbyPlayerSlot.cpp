// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerSlot.h"

#include "Lobby/FTLobbyPlayerSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"

void UFTLobbyPlayerSlot::UpdateSlotData(const FString& InPlayerName, EFTCharacterType InCharType, bool bIsReady, int32 PlayerIndex)
{
	// 1. 오직 '이름이 비어있는가?'만으로 접속 여부(빈 슬롯 여부)를 판별합니다.[cite: 10]
	bool bIsEmptySlot = InPlayerName.IsEmpty();

	// 2. 스위처 상태 즉시 전환 (접속했다면 아무것도 안 골랐어도 1번 인덱스로 갑니다)[cite: 10]
	if (Switcher_SlotState)
	{
		Switcher_SlotState->SetActiveWidgetIndex(bIsEmptySlot ? 0 : 1);
	}

	// 3. 빈 슬롯(0번 인덱스)일 때의 초기화 처리
	if (bIsEmptySlot)
	{
		if (Text_PlayerName)
		{
			Text_PlayerName->SetText(FText::FromString(TEXT("대기 중...")));
		}
		if (Img_HeroPortrait)
		{
			Img_HeroPortrait->SetBrushFromTexture(nullptr);
		}
		if (Img_TeamColor)
		{
			Img_TeamColor->SetColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 0.0f)); // 어두운 반투명 회색
		}
	}
	// 4. 유저가 접속해 있는 슬롯(1번 인덱스)일 때의 데이터 처리
	else
	{
		// 이름 세팅[cite: 10]
		if (Text_PlayerName)
		{
			Text_PlayerName->SetText(FText::FromString(InPlayerName));
		}
		
		
		if (Text_ReadyState)
		{
			if (bIsReady)
			{
				Text_ReadyState->SetText(FText::FromString(TEXT("준비 완료")));
				Text_ReadyState->SetColorAndOpacity(FSlateColor(FLinearColor::Green)); // 초록색 강조
			}
			else
			{
				Text_ReadyState->SetText(FText::FromString(TEXT("대기 중")));
				Text_ReadyState->SetColorAndOpacity(FSlateColor(FLinearColor::White)); // 기본색
			}
		}
		
		if (Img_TeamColor)
		{
			FLinearColor TintColor = FLinearColor::White;
			
			if (PlayerIndex % 2 == 0) //PlayerIndex는 0번 부터 시작
			{
				TintColor = FLinearColor(0.1f, 0.3f, 1.0f, 1.0f); // 1,3P: 파랑
			}
			else
			{
				TintColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f); // 2,4P: 빨강
			}
			
			// Image 컴포넌트의 틴트 색상은 SetColorAndOpacity로 제어합니다.
			Img_TeamColor->SetColorAndOpacity(TintColor);
		}
		
		// 캐릭터 타입에 따른 초상화 세팅[cite: 10]
		if (Img_HeroPortrait)
		{
			bool bPortraitSet = false;

			// PortraitMap에서 캐릭터 타입(InCharType)과 일치하는 이미지가 있는지 찾습니다.[cite: 10]
			if (const TSoftObjectPtr<UTexture2D>* SoftTexture = PortraitMap.Find(InCharType))
			{
				if (UTexture2D* LoadedTexture = SoftTexture->LoadSynchronous())
				{
					Img_HeroPortrait->SetBrushFromTexture(LoadedTexture);
					bPortraitSet = true;
				}
			}
			
			// 아직 None 상태이거나(맵에 None이 등록 안 된 경우) 텍스처 로드에 실패하면 빈 이미지로 둡니다.[cite: 10]
			// (에디터 PortraitMap에 None일 때 보여줄 기본 '물음표' 이미지를 등록해두면 그 이미지가 뜹니다!)
			if (!bPortraitSet)
			{
				Img_HeroPortrait->SetBrushFromTexture(nullptr); 
			}
		}
	}
}