// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Character/FTCharacterTypes.h" // 정본 EFTCharacterType 참조
#include "FTPlayerState.generated.h"

class UFT_AbilitySystemComponent;
class UFT_AttributeSet;

UENUM(BlueprintType)
enum class EFTTeam : uint8
{
	Blue UMETA(DisplayName = "Blue Team"),
	Red  UMETA(DisplayName = "Red Team"),
};


UCLASS()
class FIERYTALE_API AFTPlayerState  : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFTPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFT_AttributeSet* GetAttributeSet() const { return AttributeSet; }

	// GameMode → PlayerController → 여기로 전달되는 팀 태그 바인딩 함수
	void AssignTeamTag(EFTTeam InTeam);

	void SetSelectedCharacterType(EFTCharacterType InType) { SelectedCharacterType = InType; }
	EFTCharacterType GetSelectedCharacterType() const { return SelectedCharacterType; }

	// 실제로 스폰된 캐릭터 인스턴스
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character")
	TObjectPtr<APawn> SpawnedCharacter;

private:
	// 에디터 Detail에서 현재 선택된 캐릭터 타입 확인용 (읽기 전용)
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	EFTCharacterType SelectedCharacterType = EFTCharacterType::None;

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
