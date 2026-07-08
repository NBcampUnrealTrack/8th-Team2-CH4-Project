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
#include "TimerManager.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags/FTTags.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "AbilitySystem/Abilities/Player/Data/FTSkillMetaData.h"
#include "Engine/SkeletalMesh.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"

DEFINE_LOG_CATEGORY(FTPlayerCharacter);

namespace
{
	//	캐릭터 행의 스킬 슬롯(FFTSkillMetaData 행)에서 AbilityClass를 꺼내 ASC에 부여하고,
	//	버튼에 대응하는 기존 인풋 태그를 스펙에 실어 ActivateAbilityByInputTag가 매칭할 수 있게 한다.
	void GrantSkillFromSlot(UAbilitySystemComponent* ASC, UObject* SourceObject,
		const FDataTableRowHandle& SlotHandle, const FGameplayTag& InputTag)
	{
		if (!ASC || SlotHandle.IsNull())
		{
			return;
		}

		const FFTSkillMetaData* Row = SlotHandle.GetRow<FFTSkillMetaData>(TEXT("GrantSkillFromSlot"));
		if (!Row || !Row->AbilityClass)
		{
			return;
		}

		FGameplayAbilitySpec Spec(Row->AbilityClass, 1, -1, SourceObject);
		Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
		ASC->GiveAbility(Spec);
	}
}

void AFTPlayerCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//	[이관/폐기 보존] DOREPLIFETIME(AFTPlayerCharacterBase, CharacterData);
	DOREPLIFETIME(AFTPlayerCharacterBase, CharacterRow);
	DOREPLIFETIME(AFTPlayerCharacterBase, ReplicatedCharacterScale);
}

void AFTPlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterVisuals();
}

//	[이관/폐기 보존] 구 OnRep_CharacterData.
//void AFTPlayerCharacterBase::OnRep_CharacterData()
//{
//	ApplyCharacterVisuals();
//}

void AFTPlayerCharacterBase::OnRep_CharacterRow()
{
	ApplyCharacterVisuals();
}

void AFTPlayerCharacterBase::ApplyCharacterVisuals()
{
	//	[이관/폐기 보존] 구 UFT_CharacterData 기반 적용.
	//if (!CharacterData)
	//{
	//	return;
	//}
	//GetCharacterMovement()->MaxWalkSpeed = CharacterData->GetDefaultMoveSpeed();
	//if (USkeletalMesh* CharacterMesh = CharacterData->GetCharacterMesh())
	//{
	//	GetMesh()->SetSkeletalMesh(CharacterMesh);
	//}

	const FFTCharacterData* Data = GetCharacterData();
	if (!Data)
	{
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = Data->DefaultMoveSpeed;

	//	메쉬/애님BP는 소프트 참조 — 영웅이 확정된 이 시점에 동기 로드해서 적용한다.
	if (USkeletalMesh* CharacterMesh = Data->SkeletalMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(CharacterMesh);

		// 1. 메쉬 에셋 자체의 원본 바운드 정보 가져오기
		FBoxSphereBounds MeshBounds = CharacterMesh->GetBounds();
		float MeshOriginalHeight = MeshBounds.BoxExtent.Z * 2.0f;

		if (MeshOriginalHeight > 0.0f)
		{
			// 2. 캡슐 절반 높이 가져오기 (기본값: 96.0f)
			float CapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
			float TargetHeight = CapsuleHalfHeight * 2.0f;

			// 3. 목표 높이 대비 메쉬 원본 높이의 비율 계산
			float TargetScale = TargetHeight / MeshOriginalHeight;

			// 4. 균등 스케일 적용 (X, Y, Z 모두 동일 비율로 축소/확대)
			GetMesh()->SetRelativeScale3D(FVector(TargetScale));

			// 5. 스케일이 적용된 메쉬를 캡슐 바닥에 정확히 정렬
			// 언리얼 캐릭터 메쉬의 기본 회전축인 -90도(Z)도 함께 처리
			const FVector NewMeshRelativeLocation(0.f, 0.f, -CapsuleHalfHeight);
			const FRotator NewMeshRelativeRotation(0.f, -90.f, 0.f);
			GetMesh()->SetRelativeLocationAndRotation(NewMeshRelativeLocation, NewMeshRelativeRotation);

			// 6. PostInitializeComponents에서 이미 캐싱된 BaseTranslationOffset/BaseRotationOffset을
			// 방금 바뀐 메쉬 트랜스폼 기준으로 갱신한다.
			// 이걸 안 하면 원격 클라이언트(Simulated Proxy)의 네트워크 스무딩이 옛 오프셋 기준으로
			// 메쉬를 재배치해서, 캡슐 위치와 어긋나 붕 뜬 것처럼 보인다.
			CacheInitialMeshOffset(NewMeshRelativeLocation, NewMeshRelativeRotation);

			// 7. SetCharacterScale의 기준값으로 캐싱 — 이 시점의 캡슐/메시 배치가 이 캐릭터의 "100% 크기"다.
			DefaultCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
			DefaultCapsuleHalfHeight = CapsuleHalfHeight;
			DefaultMeshRelativeScale = GetMesh()->GetRelativeScale3D();
			DefaultMeshRelativeLocation = NewMeshRelativeLocation;
		}

	}
	if (UClass* AnimBPClass = Data->AnimClass.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClass);
	}
}

AFTPlayerCharacterBase::AFTPlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	//	이동 방향과 무관하게 항상 컨트롤러(카메라) Yaw만 바라본다. CharacterMovementComponent의 표준 리플리케이션 경로를 타야
	//	멀티플레이에서 다른 클라이언트에게도 회전이 정상 전파된다 (수동 SetActorRotation은 서버로 전달되지 않음).
	GetCharacterMovement()->bOrientRotationToMovement = false;
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
	DefaultFov = FollowCamera->FieldOfView;

	FovTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("FovTimeline"));
}

UAbilitySystemComponent* AFTPlayerCharacterBase::GetAbilitySystemComponent() const
{
	AFTPlayerState* PS = GetPlayerState<AFTPlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

const FFTCharacterData* AFTPlayerCharacterBase::GetCharacterData() const
{
	if (CharacterRow.IsNull())
	{
		return nullptr;
	}
	return CharacterRow.GetRow<FFTCharacterData>(TEXT("AFTPlayerCharacterBase::GetCharacterData"));
}

UFT_WeaponData* AFTPlayerCharacterBase::GetWeaponData() const
{
	//	[이관/폐기 보존] 구: return CharacterData ? CharacterData->GetWeaponData() : CurrentWeaponData;
	if (const FFTCharacterData* Data = GetCharacterData())
	{
		if (Data->WeaponData)
		{
			return Data->WeaponData;
		}
	}
	return CurrentWeaponData;
}

void AFTPlayerCharacterBase::PlayCameraFovEffect(float TargetFov, float Duration, UCurveFloat* Curve)
{
	//	FOV는 소유 클라이언트 화면에만 의미가 있는 순수 로컬 연출 — 서버/다른 클라이언트에서는 재생하지 않는다.
	if (!IsLocallyControlled())
	{
		return;
	}

	if (!FollowCamera || !FovTimeline || Duration <= 0.f)
	{
		return;
	}

	UCurveFloat* CurveToUse = Curve;
	if (!CurveToUse)
	{
		if (!DefaultLinearFovCurve)
		{
			DefaultLinearFovCurve = NewObject<UCurveFloat>(this);
			DefaultLinearFovCurve->FloatCurve.AddKey(0.f, 0.f);
			DefaultLinearFovCurve->FloatCurve.AddKey(1.f, 1.f);
		}
		CurveToUse = DefaultLinearFovCurve;
	}

	FovEffectStartValue = FollowCamera->FieldOfView;
	FovEffectTargetValue = TargetFov;

	static const FName FovTrackName(TEXT("FovTrack"));

	if (!bFovTrackInitialized)
	{
		//	트랙은 최초 1회만 등록 — 이후엔 SetFloatCurve로 커브만 교체한다 (반복 호출 시 트랙 중복 방지)
		FOnTimelineFloat ProgressFunction;
		ProgressFunction.BindUFunction(this, FName("OnFovTimelineUpdate"));
		FovTimeline->AddInterpFloat(CurveToUse, ProgressFunction, NAME_None, FovTrackName);
		bFovTrackInitialized = true;
	}
	else
	{
		FovTimeline->SetFloatCurve(CurveToUse, FovTrackName);
	}

	FovTimeline->SetTimelineLength(Duration);
	FovTimeline->SetTimelineLengthMode(TL_TimelineLength);
	FovTimeline->SetLooping(false);
	FovTimeline->SetPlayRate(1.f);
	FovTimeline->PlayFromStart();
}

void AFTPlayerCharacterBase::OnFovTimelineUpdate(float Alpha)
{
	if (FollowCamera)
	{
		FollowCamera->FieldOfView = FMath::Lerp(FovEffectStartValue, FovEffectTargetValue, Alpha);
	}
}

UCurveFloat* AFTPlayerCharacterBase::GetOrBuildFovCurve(TObjectPtr<UCurveFloat>& CurveCache, const TArray<FVector2D>& Keys)
{
	if (!CurveCache)
	{
		CurveCache = NewObject<UCurveFloat>(this);
		for (const FVector2D& Key : Keys)
		{
			CurveCache->FloatCurve.AddKey(Key.X, Key.Y);
		}
	}
	return CurveCache;
}

void AFTPlayerCharacterBase::PlayCameraFovPunch(float TargetFov, float PunchDuration, float HoldDuration, float ReturnDuration, UCurveFloat* PunchCurve)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	PlayCameraFovEffect(TargetFov, PunchDuration, PunchCurve);

	//	펀치인이 끝나고 HoldDuration만큼 버틴 뒤 자동으로 기본 FOV로 복귀시킨다.
	GetWorldTimerManager().ClearTimer(FovResetTimerHandle);
	GetWorldTimerManager().SetTimer(
		FovResetTimerHandle,
		FTimerDelegate::CreateUObject(this, &AFTPlayerCharacterBase::ResetCameraFov, ReturnDuration),
		FMath::Max(PunchDuration + HoldDuration, 0.01f),
		false);
}

void AFTPlayerCharacterBase::ResetCameraFov(float Duration)
{
	if (!IsLocallyControlled() || !FollowCamera)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(FovResetTimerHandle);

	if (Duration <= 0.f)
	{
		if (FovTimeline)
		{
			FovTimeline->Stop();
		}
		FollowCamera->FieldOfView = DefaultFov;
		return;
	}

	PlayCameraFovEffect(DefaultFov, Duration);
}

void AFTPlayerCharacterBase::SetCharacterScale(float Scale)
{
	if (HasAuthority())
	{
		ReplicatedCharacterScale = Scale;
	}

	ApplyCharacterScaleVisual(Scale);
}

void AFTPlayerCharacterBase::ResetCharacterScale()
{
	SetCharacterScale(1.f);
}

void AFTPlayerCharacterBase::OnRep_CharacterScale()
{
	ApplyCharacterScaleVisual(ReplicatedCharacterScale);
}

void AFTPlayerCharacterBase::ApplyCharacterScaleVisual(float Scale)
{
	//	캡슐은 SetCapsuleSize(절대값)로만 조절한다. 액터 스케일(SetActorScale3D)까지 같이 걸면 캡슐이
	//	RootComponent라 스케일이 이중으로 곱해져, 콜리전은 Scale^2배로 줄어드는데 메시는 Scale배만 줄어들어
	//	발이 바닥 아래로 파고드는 문제가 생긴다.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(DefaultCapsuleRadius * Scale, DefaultCapsuleHalfHeight * Scale, true);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeScale3D(DefaultMeshRelativeScale * Scale);
		MeshComp->SetRelativeLocation(FVector(DefaultMeshRelativeLocation.X, DefaultMeshRelativeLocation.Y, DefaultMeshRelativeLocation.Z * Scale));
	}
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

	
	// TODO:: 테스트용 - 평타관련 
	static const TArray<FVector2D> AttackFovKeys = { FVector2D(0.f, 0.f), FVector2D(0.35f, 1.f), FVector2D(1.f, 0.85f) };
	PlayCameraFovPunch(85.f, 0.08f, 0.02f, 0.15f, GetOrBuildFovCurve(AttackFovCurve, AttackFovKeys));
}

void AFTPlayerCharacterBase::OnRightClick()
{
	ActivateAbilityByInputTag(FTTags::FTAbilities::AttackSkill, true);
}

void AFTPlayerCharacterBase::OnPressQ()
{
	ActivateAbilityByInputTag(FTTags::FTAbilities::UltimateSkill, true);

	// TODO:: 테스트용, 스킬 종류따라 적용하도록 수정해야 함
	static const TArray<FVector2D> UltimateFovKeys =
	{
		FVector2D(0.f,   0.f),
		FVector2D(0.15f, -0.1f),
		FVector2D(0.5f,  1.1f),
		FVector2D(0.75f, 0.9f),
		FVector2D(1.f,   1.f)
	};
	PlayCameraFovPunch(60.f, 0.5f, 0.3f, 0.5f, GetOrBuildFovCurve(UltimateFovCurve, UltimateFovKeys));
}

void AFTPlayerCharacterBase::OnShift()
{
	ActivateAbilityByInputTag(FTTags::FTAbilities::UtilSkill, true);

	// TODO:: 테스트용, 스킬 종류따라 적용하도록 수정해야 함
	static const TArray<FVector2D> UtilFovKeys =
	{
		FVector2D(0.f,   0.f),
		FVector2D(0.3f,  1.15f),
		FVector2D(0.65f, 0.9f),
		FVector2D(1.f,   1.f)
	};
	PlayCameraFovPunch(100.f, 0.15f, 0.1f, 0.25f, GetOrBuildFovCurve(UtilFovCurve, UtilFovKeys));
}

void AFTPlayerCharacterBase::Revive()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

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

				// [이관/폐기 보존] 구: UFT_CharacterData의 인풋 태그별 어빌리티 맵(CharacterAbilities)을 순회 부여.
				//if (CharacterData)
				//{
				//	for (const TPair<FGameplayTag, TSubclassOf<UGameplayAbility>>& AbilityPair : CharacterData->GetHeroAbilities())
				//	{
				//		if (AbilityPair.Value)
				//		{
				//			FGameplayAbilitySpec Spec(AbilityPair.Value, 1, -1, this);
				//			Spec.GetDynamicSpecSourceTags().AddTag(AbilityPair.Key);
				//			ASC->GiveAbility(Spec);
				//		}
				//	}
				//}

				// 신: 캐릭터 행(FFTCharacterData)의 4개 버튼 슬롯에서 스킬을 부여.
				// 버튼→InputTag는 기존 태그를 재사용한다. (LMB→NormalAttack / RMB→AttackSkill / Space→UtilSkill / R→UltimateSkill)
				if (const FFTCharacterData* Data = GetCharacterData())
				{
					GrantSkillFromSlot(ASC, this, Data->LMBSkill,   FTTags::FTAbilities::NormalAttack);
					GrantSkillFromSlot(ASC, this, Data->RMBSkill,   FTTags::FTAbilities::AttackSkill);
					GrantSkillFromSlot(ASC, this, Data->SpaceSkill, FTTags::FTAbilities::UtilSkill);
					GrantSkillFromSlot(ASC, this, Data->RSkill,     FTTags::FTAbilities::UltimateSkill);
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
	//	[이관/폐기 보존] 구 UFT_CharacterData 기반 스탯 초기화.
	//if (!CharacterData || !HasAuthority())
	//{
	//	return;
	//}
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),        CharacterData->GetDefaultMaxHealth());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),           CharacterData->GetDefaultMaxHealth());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxShieldAttribute(),        CharacterData->GetDefaultMaxShield());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetShieldAttribute(),           CharacterData->GetDefaultMaxShield());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxMoveSpeedAttribute(),     CharacterData->GetDefaultMoveSpeed());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),        CharacterData->GetDefaultMoveSpeed());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(),      CharacterData->GetDefaultAttackPower());
	//ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxUltimateGaugeAttribute(), CharacterData->GetDefaultMaxUltimateGauge());

	const FFTCharacterData* Data = GetCharacterData();
	if (!Data || !HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxHealthAttribute(),        Data->DefaultMaxHealth);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetHealthAttribute(),           Data->DefaultMaxHealth);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxShieldAttribute(),        Data->DefaultMaxShield);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetShieldAttribute(),           Data->DefaultMaxShield);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxMoveSpeedAttribute(),     Data->DefaultMoveSpeed);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMoveSpeedAttribute(),        Data->DefaultMoveSpeed);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetAttackPowerAttribute(),      Data->DefaultAttackPower);
	ASC->SetNumericAttributeBase(UFT_AttributeSet::GetMaxUltimateGaugeAttribute(), Data->DefaultMaxUltimateGauge);
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
