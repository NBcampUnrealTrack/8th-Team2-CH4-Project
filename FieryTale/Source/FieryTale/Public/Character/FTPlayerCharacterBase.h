// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/FTCharacterBase.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "AbilitySystem/Abilities/Player/Character/FT_CharacterData.h"
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
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
	// TODO:: GetWeaponData시 CharacterData->GetWeaponData(); 따로 필드에 이 값을 갖고 있을 필요성이 있는가?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_WeaponData> CurrentWeaponData;

	virtual void BeginPlay() override;

public:
	AFTPlayerCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;

	// ASC는 PlayerState에서 가져옴
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(ReplicatedUsing = OnRep_CharacterData, EditAnywhere, BlueprintReadWrite, Category = "FieryTale | Character")
	TObjectPtr<UFT_CharacterData> CharacterData;

	UFUNCTION()
	void OnRep_CharacterData();
	
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Weapon")
	UFT_WeaponData* GetWeaponData() const 
	{  
		if (CharacterData)
		{
			return CharacterData->GetWeaponData();	
		}
		
		return CurrentWeaponData;
	}

	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void Revive();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	// FTPlayerController에서 호출 — TODO:: 함수명이 부적절하여, 기능 단위로 이름을 고쳐야함
	virtual void OnLeftClick();
	virtual void OnRightClick();
	virtual void OnPressQ();
	virtual void OnShift();
	
	void DebugDie(); // TODO:: 삭제 예정

protected:

	void ActivateAbilityByInputTag(const FGameplayTag& InputTag, bool bIsPressed) const;

	// CharacterData 기반으로 메시·이동속도 적용 — BeginPlay 및 OnRep_CharacterData에서 호출
	void ApplyCharacterVisuals();

	// 초기 혹은 Respawn 단계에서 CharacterData에 의거하여 Attribute 값 초기화
	void InitializeCharacterAttribute() const;

private:
	// 이동 방향과 무관하게 항상 컨트롤러(카메라) 방향을 바라보도록 회전 적용 (사이퍼즈 식 이동)
	void UpdateFacingRotation();
};
