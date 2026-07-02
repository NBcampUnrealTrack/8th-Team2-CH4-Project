// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/FTPlayerCharacterBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "InputActionValue.h"
#include "Character/FTPlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags/FTTags.h"

DEFINE_LOG_CATEGORY(FTPlayerCharacter);

void AFTPlayerCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFTPlayerCharacterBase, CharacterData);
}

void AFTPlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterVisuals();
}

void AFTPlayerCharacterBase::OnRep_CharacterData()
{
	ApplyCharacterVisuals();
}

void AFTPlayerCharacterBase::ApplyCharacterVisuals()
{
	if (!CharacterData)
	{
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = CharacterData->GetDefaultMoveSpeed();

	if (USkeletalMesh* CharacterMesh = CharacterData->GetCharacterMesh())
	{
		GetMesh()->SetSkeletalMesh(CharacterMesh);
	}
}

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
	//GetCharacterMovement()->AirControl = 0.35f;
	//GetCharacterMovement()->MaxWalkSpeed = 500.f;
	//GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	//GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	//GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 40.0f, 60.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 90.0f;
}

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

void AFTPlayerCharacterBase::OnLeftClick()
{
	UE_LOG(FTPlayerCharacter, Display, TEXT("LeftClick"));
	ActivateAbilityByInputTag(FTTags::FTAbilities::NormalAttack, true);
}

void AFTPlayerCharacterBase::OnRightClick()
{
}

void AFTPlayerCharacterBase::OnShift()
{
}

void AFTPlayerCharacterBase::Revive()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	InitializeCharacterAttribute();
}

// TODO:: 사망 확인을 위한 임시코드 삭제 예정
void AFTPlayerCharacterBase::DebugDie()
{
	UE_LOG(FTPlayerCharacter, Display, TEXT("Called Die For DeBugging"));
	Die();
}

void AFTPlayerCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	const bool bIsLocal = IsLocallyControlled();
	CameraBoom->SetActive(bIsLocal);
	FollowCamera->SetActive(bIsLocal);
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
            
			// 2. 서버 권한 하에 초기화 수행
			if (HasAuthority())
			{
				InitializeCharacterAttribute();

				// 기획자가 에디터에서 배치한 스킬 및 평타 목록을 엔진에 등록(Grant)
				for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
				{
					if (AbilityClass)
					{
						ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, -1, this));
					}
				}

				// CharacterData의 인풋 태그별 어빌리티 맵(CharacterAbilities)을 부여.
				// C++ 클래스 직접 스폰 경로에서는 StartupAbilities가 비어 있으므로,
				// 데이터 에셋(DA_*)에 채운 스킬셋이 실제 부여 경로가 된다.
				if (CharacterData)
				{
					for (const TPair<FGameplayTag, TSubclassOf<UGameplayAbility>>& AbilityPair : CharacterData->GetHeroAbilities())
					{
						if (AbilityPair.Value)
						{
							FGameplayAbilitySpec Spec(AbilityPair.Value, 1, -1, this);
							// CharacterData 맵의 키(입력 태그)를 스펙에 실어, 어빌리티 자체의 AssetTags 세팅 여부와 무관하게
							// ActivateAbilityByInputTag에서 이 태그로 매칭할 수 있게 한다.
							Spec.GetDynamicSpecSourceTags().AddTag(AbilityPair.Key);
							ASC->GiveAbility(Spec);
						}
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
			ASC->InitAbilityActorInfo(PS, this);
		}
	}
}

void AFTPlayerCharacterBase::InitializeCharacterAttribute() const
{
	if (!CharacterData || !HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),        CharacterData->GetDefaultMaxHealth());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),           CharacterData->GetDefaultMaxHealth());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxShieldAttribute(),        CharacterData->GetDefaultMaxShield());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetShieldAttribute(),           CharacterData->GetDefaultMaxShield());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxMoveSpeedAttribute(),     CharacterData->GetDefaultMoveSpeed());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),        CharacterData->GetDefaultMoveSpeed());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(),      CharacterData->GetDefaultAttackPower());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxUltimateGaugeAttribute(), CharacterData->GetDefaultMaxUltimateGauge());
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetUltimateGaugeAttribute(),    0.0f);

	// Die()에서 부여한 Dead 태그를 해제한다 (부여 시와 동일한 복제 상태로 대칭 호출).
	ASC->RemoveLooseGameplayTag(FTTags::FTStates::Core::Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
}

void AFTPlayerCharacterBase::ActivateAbilityByInputTag(const FGameplayTag& InputTag, bool bIsPressed) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (bIsPressed)
	{
		// AssetTags(에셋에 고정 박힌 태그)뿐 아니라, CharacterData 맵에서 Grant 시점에 실어준
		// DynamicSpecSourceTags(입력 태그)도 함께 검사해야 매칭된다.
		FGameplayAbilitySpecHandle MatchedHandle;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability &&
				(Spec.Ability->GetAssetTags().HasTagExact(InputTag) || Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				MatchedHandle = Spec.Handle;
				UE_LOG(FTPlayerCharacter, Display, TEXT("[ActivateAbilityByInputTag] Matched GA: %s"), *Spec.Ability->GetName());
				break;
			}
		}

		bool bActivated = false;
		if (MatchedHandle.IsValid())
		{
			// TryActivateAbilitiesByTag는 AssetTags만 검사하므로 DynamicSpecSourceTags로 찾은 매칭은
			// 놓치게 된다. 위에서 찾은 핸들로 직접 활성화한다.
			bActivated = ASC->TryActivateAbility(MatchedHandle);
		}
		else
		{
			UE_LOG(FTPlayerCharacter, Warning, TEXT("[ActivateAbilityByInputTag] No GA found with Tag=%s. GrantedAbilities=%d"), *InputTag.ToString(), ASC->GetActivatableAbilities().Num());
		}
		UE_LOG(FTPlayerCharacter, Display, TEXT("[ActivateAbilityByInputTag] Tag=%s | Result=%s"), *InputTag.ToString(), bActivated ? TEXT("Success") : TEXT("Failed"));
	}
	else
	{
		FGameplayTagContainer CancelContainer(InputTag);
		ASC->CancelAbilities(&CancelContainer);
	}
}
