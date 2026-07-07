// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTCharacterSelectButton.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Character/FTPlayerController.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "Engine/Texture2D.h"

void UFTCharacterSelectButton::InitializeButton(const FFTCharacterData* InCharData, AFTPlayerController* InPC)
{
	if (!InCharData || !InPC) return;

	// 1. 데이터 저장 (클릭 시 사용)
	LobbyPC = InPC;
	StoredCharacterType = InCharData->CharacterType;

	// 2. UI 텍스트 업데이트 (영웅 이름)
	if (Text_Name)
	{
		Text_Name->SetText(InCharData->DisplayName);
	}

	// 3. UI 이미지 업데이트 (초상화 텍스처 비동기/동기 로드)
	if (Img_Portrait && !InCharData->PortraitIcon.IsNull())
	{
		if (UTexture2D* LoadedTexture = InCharData->PortraitIcon.LoadSynchronous())
		{
			Img_Portrait->SetBrushFromTexture(LoadedTexture);
		}
	}

	// 4. 클릭 이벤트 바인딩 (안전하게 기존 바인딩 제거 후 추가)
	if (Btn_Character)
	{
		Btn_Character->OnClicked.RemoveDynamic(this, &UFTCharacterSelectButton::OnButtonClicked);
		Btn_Character->OnClicked.AddDynamic(this, &UFTCharacterSelectButton::OnButtonClicked);
	}
}

void UFTCharacterSelectButton::OnButtonClicked()
{
	// 버튼이 눌리면 저장해둔 컨트롤러를 통해 서버로 내 캐릭터를 변경해달라고 RPC 요청을 보냅니다!
	if (LobbyPC && StoredCharacterType != EFTCharacterType::None)
	{
		LobbyPC->RequestSetCharacter(StoredCharacterType);
	}
}