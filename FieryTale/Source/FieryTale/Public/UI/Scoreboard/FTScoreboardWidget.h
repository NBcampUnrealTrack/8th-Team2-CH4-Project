// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FTScoreboardWidget.generated.h"

class UVerticalBox;
class UDataTable;
class UFTScoreboardRowWidget;
class AFTPlayerState;
enum class EFTCharacterType : uint8;
struct FFTCharacterData;

/**
 *	인게임(Arena) 스코어보드 베이스 클래스. TAB을 누르고 있는 동안만 표시된다
 *	(표시/숨김은 컨트롤러의 ScoreboardAction Press/Release에서 가시성 토글로 처리).
 *
 *	GameState의 PlayerArray를 순회해 접속 중인 모든 플레이어의 행을 만들고,
 *	팀(Blue/Red)에 따라 두 VerticalBox 중 하나에 넣는다. 각 행의 K/D는 복제된
 *	PlayerState(Kills/Deaths)에서 읽으므로, 각 클라이언트가 다른 플레이어의 값도 본다.
 *
 *	BP에서 이 클래스를 상속해 레이아웃을 구성하고 아래를 지정한다:
 *	  - Box_BlueTeam (UVerticalBox, BindWidget) : 블루팀 행 컨테이너
 *	  - Box_RedTeam  (UVerticalBox, BindWidget) : 레드팀 행 컨테이너
 *	  - RowWidgetClass      : 각 플레이어 행으로 생성할 Row 위젯 BP
 *	  - CharacterDataTable  : 초상화/캐릭터 이름을 조회할 DT_CharacterData
 */
UCLASS()
class FIERYTALE_API UFTScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//	표시 중일 때만: 인원이 바뀌면 행을 다시 만들고, 아니면 K/D 숫자만 갱신한다.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* Box_BlueTeam;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* Box_RedTeam;

	//	각 플레이어 행으로 생성할 Row 위젯 클래스 (UFTScoreboardRowWidget 상속 BP).
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Scoreboard")
	TSubclassOf<UFTScoreboardRowWidget> RowWidgetClass;

	//	초상화(PortraitIcon)와 캐릭터 표시 이름(DisplayName)을 조회할 캐릭터 데이터테이블.
	//	행 이름은 EFTCharacterType 값 이름과 일치한다는 규약(스폰 로직과 동일)을 따른다. 
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale|Scoreboard", meta = (RowType = "FTCharacterData"))
	TObjectPtr<UDataTable> CharacterDataTable;

private:
	//	현재 표시 중인 행들과 그 소유 PlayerState를 같은 인덱스로 정렬 보관한다.
	//	인원 변화가 없으면 이 캐시로 재생성 없이 점수만 갱신한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<AFTPlayerState>> CachedPlayers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFTScoreboardRowWidget>> CachedRows;

	//	마지막으로 행을 만든 시점의 인원수. PlayerArray 수와 다르면 재생성 트리거.
	int32 CachedPlayerCount = -1;

	//	PlayerArray를 순회해 팀별 행을 처음부터 다시 만든다(입장/퇴장 반영).
	void RebuildRows();

	//	기존 행의 K/D만 최신 PlayerState 값으로 갱신한다.
	void RefreshScores();

	//	캐릭터 타입 → DT_CharacterData 행. 테이블/행이 없으면 nullptr.
	const FFTCharacterData* FindCharacterData(EFTCharacterType Type) const;
};
