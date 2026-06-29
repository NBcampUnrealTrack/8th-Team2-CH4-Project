// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"

#include "FTPlayerCharacterBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UGameplayAbility;
class UGameplayEffect; // ◄◄◄ 이펙트 클래스 처리를 위한 전방 선언 추가


DECLARE_LOG_CATEGORY_EXTERN(FTPlayerCharacter, Log, All);

UCLASS()
class FIERYTALE_API AFTPlayerCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	/** Left Click Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LeftClickAction;

	/** Right Click Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* RightClickAction;

	/** Shift Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* ShiftAction;

	// =============================================================================
	// [WEAPON SYSTEM DATA ASSET]
	// =============================================================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_WeaponData> CurrentWeaponData;

	virtual void BeginPlay() override;
	
public:
	AFTPlayerCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// [인터페이스 구현] PlayerState를 추적하여 ASC를 중앙 집중식으로 반환합니다.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 평타 어빌리티(GA)가 실시간으로 캐릭터의 무기 스펙을 동적으로 읽어갈 수 있도록 열어주는 Getter /
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Weapon")
	UFT_WeaponData* GetWeaponData() const { return CurrentWeaponData; }
	
protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	virtual void OnLeftClick(const FInputActionValue& Value);
	virtual void OnRightClick(const FInputActionValue& Value);
	virtual void OnShift(const FInputActionValue& Value);

	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;
	
	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	// =============================================================================
	// [ 레퍼런스 스타일을 반영한 GAS 기획 노출 변수 구역]
	// =============================================================================
	/* 기획자가 에디터 디테일 창에서 이 캐릭터에게 쥐어줄 기본 스킬/평타 목록입니다. (예: GA_RRD_NormalAttack) */
	UPROPERTY(EditDefaultsOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/* 캐릭터 생성 시 기본 스탯 주입 및 캐릭터 식별 태그를 일괄 부여하기 위한 기본 게임플레이 이펙트 리스트입니다. 
     기획자가 GE_Initialize_RedRidingHood 같은 자산을 디테일 창에 툭 넣어주면 끝납니다! */
  UPROPERTY(EditDefaultsOnly, Category = "FieryTale | GAS", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UGameplayEffect>> DefaultGameplayEffects;	

public:	
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

};
