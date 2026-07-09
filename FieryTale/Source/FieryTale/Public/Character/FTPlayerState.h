// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Character/FTCharacterTypes.h"
#include "FTPlayerState.generated.h"

class UFT_AbilitySystemComponent;
class UFT_AttributeSet;



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

	// 복제되어 클라이언트에서 OnRep_Team을 트리거함
	UPROPERTY(ReplicatedUsing = OnRep_Team)
	EFTTeam Team = EFTTeam::Blue;

	UFUNCTION()
	void OnRep_Team();

	void ApplyTeamTag() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_AttributeSet> AttributeSet;


};
