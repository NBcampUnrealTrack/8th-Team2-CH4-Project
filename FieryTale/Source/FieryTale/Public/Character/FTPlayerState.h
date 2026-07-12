// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Character/FTCharacterTypes.h"
#include "FTPlayerState.generated.h"

class UFT_AbilitySystemComponent;
class UFT_AttributeSet;

// 이 PlayerState의 팀(Blue/Red) 태그가 ASC에 확정되면 1회 발생한다.
// 팀 확정 전에 색상을 판정하려던 체력바 위젯들이 구독해뒀다가 재시도하는 용도.
DECLARE_MULTICAST_DELEGATE(FOnTeamTagReady);

UCLASS()
class FIERYTALE_API AFTPlayerState  : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UFT_AttributeSet* GetAttributeSet() const { return AttributeSet; }

	// GameMode → PlayerController → 여기로 전달되는 팀 태그 바인딩 함수
	void AssignTeamTag(EFTTeam InTeam);

	// 이 PlayerState의 팀 태그가 확정되면 1회 발생한다 (경기 중 팀이 바뀌지 않으므로 1회성 신호로 취급)
	FOnTeamTagReady OnTeamTagReady;

	UFUNCTION(BlueprintPure, Category = "FieryTale|Team")
	EFTTeam GetTeam() const { return Team; }
	
	UPROPERTY(Replicated)
	int32 PlayerIndex = -1;

	int32 GetPlayerIndex() const { return PlayerIndex; }
	void SetPlayerIndex(int32 InIndex) { PlayerIndex = InIndex; }

	void SetSelectedCharacterType(EFTCharacterType InType) { SelectedCharacterType = InType; }
	EFTCharacterType GetSelectedCharacterType() const { return SelectedCharacterType; }

	// 실제로 스폰된 캐릭터 인스턴스
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character")
	TObjectPtr<APawn> SpawnedCharacter;
	
	// 킬/데스를 올리는 함수 (서버에서만 호출)
	void AddKill();
	void AddDeath();

	// UI에서 읽어갈 Getter
	UFUNCTION(BlueprintPure, Category = "FieryTale|Score")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "FieryTale|Score")
	int32 GetDeaths() const { return Deaths; }
	
protected:
	
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

private:
	// 에디터 Detail에서 현재 선택된 캐릭터 타입 확인용 (읽기 전용)
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	EFTCharacterType SelectedCharacterType;

	UPROPERTY(Replicated)
	EFTTeam Team = EFTTeam::Blue;

	// 팀이 실제로 배정됐는지 여부. 기본값이 항상 false라서 배정 시(false→true) RepNotify가 100% 보장된다.
	UPROPERTY(ReplicatedUsing = OnRep_TeamAssigned)
	bool bTeamAssigned = false;

	UFUNCTION()
	void OnRep_TeamAssigned();

	void ApplyTeamTag();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_AttributeSet> AttributeSet;


};
