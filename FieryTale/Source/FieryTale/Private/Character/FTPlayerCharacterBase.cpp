// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/FTPlayerCharacterBase.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Character/FTPlayerState.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h" 
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"

DEFINE_LOG_CATEGORY(FTPlayerCharacter);

void AFTPlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

// Sets default values
AFTPlayerCharacterBase::AFTPlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	
	
	CameraBoom->TargetArmLength = 450.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 60.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	FollowCamera->FieldOfView = 90.0f;

}

// ──► [인터페이스 구현] 중앙 집중식 ASC 반환 체계
UAbilitySystemComponent* AFTPlayerCharacterBase::GetAbilitySystemComponent() const
{
	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void AFTPlayerCharacterBase::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AFTPlayerCharacterBase::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AFTPlayerCharacterBase::OnLeftClick(const FInputActionValue& Value)
{
	//UE_LOG(FTPlayerCharacter, Display, TEXT("LeftClick"));

}

void AFTPlayerCharacterBase::OnRightClick(const FInputActionValue& Value)
{
	//UE_LOG(FTPlayerCharacter, Display, TEXT("RightClick"));
}

void AFTPlayerCharacterBase::OnShift(const FInputActionValue& Value)
{
	//UE_LOG(FTPlayerCharacter, Display, TEXT("ShiftPressed"));
}

void AFTPlayerCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	const bool bIsLocal = IsLocallyControlled();
	CameraBoom->SetActive(bIsLocal);
	FollowCamera->SetActive(bIsLocal);

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AFTPlayerCharacterBase::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	// Action 바인딩
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFTPlayerCharacterBase::Move);
		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFTPlayerCharacterBase::Look);
		// Left Click
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &AFTPlayerCharacterBase::OnLeftClick);
		// Right Click
		EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Started, this, &AFTPlayerCharacterBase::OnRightClick);
		// Shift
		EnhancedInputComponent->BindAction(ShiftAction, ETriggerEvent::Started, this, &AFTPlayerCharacterBase::OnShift);
		
		
		
		
		
	}
	else
	{
		UE_LOG(FTPlayerCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

// [서버 인프라 초기화 및 데이터 주입]
void AFTPlayerCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
    if (PS)
    {
        UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
        if (ASC)
        {
            // 1. GAS 액터 정보 등록 (Owner: PlayerState, Avatar: 캐릭터 본체)
            ASC->InitAbilityActorInfo(PS, this);
            
            // 2. 서버 권한 하에 기획자가 에디터에서 배치한 스킬 및 평타 목록을 엔진에 등록(Grant)
            if (HasAuthority())
            {
                for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
                {
                    if (AbilityClass)
                    {
                        ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, -1, this));
                    }
                }

                // [변경 지점] 지저분한 태그 수동 주입을 제거하고, 
                // 레퍼런스 스타일의 정석대로 디테일 창의 기본 이펙트(GE)를 순회하며 일괄 발동(Apply)시킵니다.
                for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultGameplayEffects)
                {
                    if (EffectClass)
                    {
                        FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
                        EffectContext.AddSourceObject(this);

                        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
                        if (SpecHandle.IsValid())
                        {
                            // 내 자신(Avatar)에게 버프 및 스탯 이펙트를 안전하게 적용시킵니다.
                            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                        }
                    }
                }
            }
        }
    }
}

// ──► [클라이언트 인프라 동기화]
void AFTPlayerCharacterBase::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
    if (PS)
    {
        UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
        if (ASC)
        {
            // 클라이언트 사이드 예측(Prediction)과 태그 동기화를 위해 액터 정보를 매핑합니다.
            ASC->InitAbilityActorInfo(PS, this);
        }
    }
}
