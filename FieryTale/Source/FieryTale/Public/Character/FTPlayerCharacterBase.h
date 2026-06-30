// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/FTCharacterBase.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "GameplayTagContainer.h"
#include "FTPlayerCharacterBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(FTPlayerCharacter, Log, All);

UCLASS()
class FIERYTALE_API AFTPlayerCharacterBase : public AFTCharacterBase
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_WeaponData> CurrentWeaponData;

	virtual void BeginPlay() override;

public:
	AFTPlayerCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ASC는 PlayerState에서 가져옴
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "FieryTale | Weapon")
	UFT_WeaponData* GetWeaponData() const { return CurrentWeaponData; }

	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void Revive();

	// FTPlayerController에서 호출 — 추후 NormalAttack/SpecialAttack 등으로 확장 예정
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	virtual void OnLeftClick();
	virtual void OnRightClick();
	virtual void OnShift();
	void DebugDie(); // TODO:: 삭제 예정

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;

	void ActivateAbilityByInputTag(const FGameplayTag& InputTag, bool bIsPressed) const;
};
