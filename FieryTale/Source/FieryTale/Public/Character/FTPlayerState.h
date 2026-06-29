// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h" 
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

	UFT_AttributeSet* GetAttributeSet() const { return AttributeSet; }
	
	
	// 실제로 스폰된 캐릭터 인스턴스
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character")
	TObjectPtr<APawn> SpawnedCharacter;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_AttributeSet> AttributeSet;
	

};
