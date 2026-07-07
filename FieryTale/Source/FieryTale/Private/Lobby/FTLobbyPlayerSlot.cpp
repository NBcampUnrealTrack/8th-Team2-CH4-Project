// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTLobbyPlayerSlot.h"

#include "Lobby/FTLobbyPlayerSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UFTLobbyPlayerSlot::UpdateSlotData(const FString& InPlayerName, EFTCharacterType InCharType)
{
	// 1. 플레이어 이름 세팅
	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(FText::FromString(InPlayerName));
	}

	// 2. 캐릭터 선택에 따른 초상화 텍스처 변경
	if (Img_HeroPortrait)
	{
		if (const TSoftObjectPtr<UTexture2D>* SoftTexture = PortraitMap.Find(InCharType))
		{
			// 동기식 로드로 텍스처를 가져와 이미지 컴포넌트에 세팅합니다.
			if (UTexture2D* LoadedTexture = SoftTexture->LoadSynchronous())
			{
				Img_HeroPortrait->SetBrushFromTexture(LoadedTexture);
				return;
			}
		}
		
		// 만약 고른 캐릭터가 없거나(None) 맵에 없다면 투명하게 하거나 기본 이미지를 씌웁니다.
		Img_HeroPortrait->SetBrushFromTexture(nullptr); 
	}
}