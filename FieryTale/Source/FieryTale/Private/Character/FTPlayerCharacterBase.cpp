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
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayTags/FTTags.h"
#include "AbilitySystem/Abilities/Player/Data/FTCharacterData.h"
#include "AbilitySystem/Abilities/Player/Data/FTSkillMetaData.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystem/Abilities/Player/FT_PlayerDeathAbility.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "UI/Health/FTHealthWidgetComponent.h"
#include "Blueprint/UserWidget.h"

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
	DOREPLIFETIME(AFTPlayerCharacterBase, CharacterRow);
	DOREPLIFETIME(AFTPlayerCharacterBase, ReplicatedCharacterScale);
}

void AFTPlayerCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterVisuals();

	if (CameraBoom)
	{
		DefaultCameraSocketOffset = CameraBoom->SocketOffset;
		DefaultCameraArmLength = CameraBoom->TargetArmLength;
		CameraBoom->AddLocalRotation(FRotator(-60.0f, 0.0f, 0.0f));
	}
}

void AFTPlayerCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//	체력바는 BP 컴포넌트 트리에 손대지 않고 C++이 직접 만들어 붙인다 — HealthWidgetClass/
	//	HealthWidgetBaseScale/HealthWidgetZOffset은 BP_FTPlayerCharacterBase의 Class Defaults에서
	//	단순 프로퍼티로 지정한다(컴포넌트 이름 충돌/고아 오버라이드 문제 자체가 발생하지 않음).
	HealthWidgetComponent = NewObject<UFTHealthWidgetComponent>(this, TEXT("HealthWidgetComponent"));
	HealthWidgetComponent->SetupAttachment(RootComponent);
	HealthWidgetComponent->RegisterComponent();
	HealthWidgetComponent->SetRelativeScale3D(FVector(HealthWidgetBaseScale));
	HealthWidgetComponent->AdditionalZOffset = HealthWidgetZOffset;

	if (UClass* WidgetClass = HealthWidgetClass.LoadSynchronous())
	{
		HealthWidgetComponent->SetWidgetClass(WidgetClass);
	}
}

void AFTPlayerCharacterBase::OnRep_CharacterRow()
{
	ApplyCharacterVisuals();
}

void AFTPlayerCharacterBase::ApplyCharacterVisuals()
{
	const FFTCharacterData* Data = GetCharacterData();
	if (!Data)
	{
		return;
	}

	if (LastAppliedCharacterRow == CharacterRow)
	{
		return;
	}
	LastAppliedCharacterRow = CharacterRow;

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
			const AFTPlayerCharacterBase* CDO = GetClass()->GetDefaultObject<AFTPlayerCharacterBase>();
			float CapsuleHalfHeight = CDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
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
			DefaultCapsuleRadius = CDO->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
			DefaultCapsuleHalfHeight = CapsuleHalfHeight;
			DefaultMeshRelativeScale = GetMesh()->GetRelativeScale3D();
			DefaultMeshRelativeLocation = NewMeshRelativeLocation;
		}

	}
	
	if (UClass* AnimBPClass = Data->AnimClass.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClass);
	}

	//	사망 몽타주도 위 메쉬/애님BP와 동일하게 영웅별 소프트 참조라 이 시점에 동기 로드해서 덮어쓴다.
	if (UAnimMontage* LoadedDeathMontage = Data->DeathMontage.LoadSynchronous())
	{
		DeathMontage = LoadedDeathMontage;
	}

	//	스켈레탈 메쉬가 확정된 뒤라야 소켓 존재 여부를 정확히 판단할 수 있으므로 이 순서로 호출한다.
	//	Equip 부착 소켓과 FirePoint 기준점 소켓 모두 무기 메쉬가 아니라 캐릭터 스켈레톤(리깅)에 박혀 있다 —
	//	그래서 무기가 없는 캐릭터(카구야 등)도 스켈레톤만 맞으면 그대로 동작하고, 리깅에 해당 소켓이 없는
	//	캐릭터는 DoesSocketExist 검사에 걸려 기존 폴백 동작으로 자연히 빠진다.
	ApplyHandEquip(LeftHandWeaponMesh, Data->LEquip, TEXT("L_Hand_Equip"), Data->LEquipScale);
	ApplyHandEquip(RightHandWeaponMesh, Data->REquip, TEXT("R_Hand_Equip"), Data->REquipScale);
}

void AFTPlayerCharacterBase::ApplyHandEquip(UStaticMeshComponent* WeaponMeshComp, const TSoftObjectPtr<UStaticMesh>& EquipMeshRef, FName HandSocket, float EquipScale)
{
	if (!WeaponMeshComp)
	{
		return;
	}

	UStaticMesh* EquipMesh = EquipMeshRef.LoadSynchronous();
	WeaponMeshComp->SetStaticMesh(EquipMesh); // 데이터에 참조가 없으면 nullptr이 들어가 자동으로 화면에서 사라진다.

	if (EquipMesh && GetMesh()->DoesSocketExist(HandSocket))
	{
		WeaponMeshComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);

		WeaponMeshComp->SetWorldScale3D(FVector(EquipScale));
	}
}

FVector AFTPlayerCharacterBase::GetWeaponMuzzleLocation() const
{
	static const FName WeaponFirePointSocketName(TEXT("Socket_Fire_Point"));
	static const FName RightFirePointSocketName(TEXT("R_Hand_FirePoint"));
	static const FName LeftFirePointSocketName(TEXT("L_Hand_FirePoint"));

	const USkeletalMeshComponent* CharacterMesh = GetMesh();

	if (RightHandWeaponMesh && RightHandWeaponMesh->GetStaticMesh())
	{
		if (RightHandWeaponMesh->DoesSocketExist(WeaponFirePointSocketName))
		{
			return RightHandWeaponMesh->GetSocketLocation(WeaponFirePointSocketName);
		}
		if (CharacterMesh && CharacterMesh->DoesSocketExist(RightFirePointSocketName))
		{
			return CharacterMesh->GetSocketLocation(RightFirePointSocketName);
		}
	}

	if (LeftHandWeaponMesh && LeftHandWeaponMesh->GetStaticMesh())
	{
		if (LeftHandWeaponMesh->DoesSocketExist(WeaponFirePointSocketName))
		{
			return LeftHandWeaponMesh->GetSocketLocation(WeaponFirePointSocketName);
		}
		if (CharacterMesh && CharacterMesh->DoesSocketExist(LeftFirePointSocketName))
		{
			return CharacterMesh->GetSocketLocation(LeftFirePointSocketName);
		}
	}

	return GetActorLocation() + FVector(0.f, 0.f, 60.f); // 무기/FirePoint 리깅이 없는 캐릭터는 기존 기본 오프셋 그대로 사용.
}

FVector AFTPlayerCharacterBase::GetCameraAimDirection(const FVector& MuzzleLocation, float MaxTraceDistance) const
{
	// 폴백: 시점 정보를 못 구하면 기존과 동일하게 액터 정면 방향을 사용한다.
	const FVector FallbackDirection = GetActorForwardVector();

	// 시점(카메라) 위치·방향 확보. 컨트롤러의 뷰포인트는 서버로 복제되는 컨트롤 회전을 반영하므로
	// 서버 권위 실행 경로에서도 클라이언트의 조준을 근사한다.
	FVector ViewLocation;
	FRotator ViewRotation;
	if (const AController* OwningController = GetController())
	{
		OwningController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (FollowCamera)
	{
		ViewLocation = FollowCamera->GetComponentLocation();
		ViewRotation = FollowCamera->GetComponentRotation();
	}
	else
	{
		return FallbackDirection;
	}

	const FVector ViewDirection = ViewRotation.Vector();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return ViewDirection;
	}

	// 카메라 정면 라인트레이스 → 처음 충돌하는 지점을 조준점으로 잡는다.
	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = ViewLocation + (ViewDirection * MaxTraceDistance);

	FVector AimPoint = TraceEnd;
	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CameraAimTrace), /*bTraceComplex=*/false, this);
	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility, QueryParams))
	{
		AimPoint = HitResult.ImpactPoint;
	}

	// 머즐 → 조준점 방향 = 실제 투사체가 나가는 방향.
	FVector AimDirection = (AimPoint - MuzzleLocation).GetSafeNormal();

	// 근접 벽 밀착 등으로 조준점이 머즐보다 뒤에 있어 역방향이 되면 시점 정면 방향으로 대체한다.
	if (AimDirection.IsNearlyZero() || FVector::DotProduct(AimDirection, ViewDirection) < 0.f)
	{
		AimDirection = ViewDirection;
	}

	return AimDirection;
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

	//	L_Hand/R_Hand 소켓 부착 여부는 스켈레탈 메쉬가 확정되는 ApplyCharacterVisuals에서 판가름 나므로,
	//	여기서는 일단 메시 루트에 매달아두고 실제 재부착은 그쪽에서 처리한다.
	LeftHandWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandWeaponMesh"));
	LeftHandWeaponMesh->SetupAttachment(GetMesh());
	LeftHandWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightHandWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandWeaponMesh"));
	RightHandWeaponMesh->SetupAttachment(GetMesh());
	RightHandWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StartupAbilities.Add(UFT_PlayerDeathAbility::StaticClass());
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

void AFTPlayerCharacterBase::SetCameraLocation(const FVector& NewLocation)
{
	if (CameraBoom)
	{
		CameraBoom->SocketOffset = NewLocation;
	}
}

void AFTPlayerCharacterBase::ResetCameraLocation()
{
	SetCameraLocation(DefaultCameraSocketOffset);
}

void AFTPlayerCharacterBase::SetCameraDistanceScale(float Scale)
{
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = DefaultCameraArmLength * Scale;
	}
}

void AFTPlayerCharacterBase::ResetCameraDistanceScale()
{
	SetCameraDistanceScale(1.f);
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
		const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		const float NewHalfHeight = DefaultCapsuleHalfHeight * Scale;
		Capsule->SetCapsuleSize(DefaultCapsuleRadius * Scale, NewHalfHeight, true);

		if (GetLocalRole() != ROLE_SimulatedProxy)
		{
			AddActorWorldOffset(FVector(0.f, 0.f, NewHalfHeight - OldHalfHeight));
		}
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeScale3D(DefaultMeshRelativeScale * Scale);
		const FVector NewMeshRelativeLocation(DefaultMeshRelativeLocation.X, DefaultMeshRelativeLocation.Y, DefaultMeshRelativeLocation.Z * Scale);
		MeshComp->SetRelativeLocation(NewMeshRelativeLocation);

		//	SimulatedProxy 네트워크 스무딩이 BaseTranslationOffset을 기준으로 메시 위치를 주기적으로
		//	재적용하는데, 이게 스폰 시점에 캐싱된 원본 오프셋이라 스케일 변경분을 반영 못 해 몇 틱 뒤
		//	원래 크기 위치로 되돌아간다. 스케일이 바뀔 때마다 같이 갱신해줘야 한다.
		CacheInitialMeshOffset(NewMeshRelativeLocation, MeshComp->GetRelativeRotation());
	}

	//	HealthWidgetComponent는 RootComponent(캡슐)에 붙어있고 Root 자체는 스케일을 갖지 않으므로(위 주석 참고),
	//	위젯의 RelativeScale3D를 Scale로 절대 대입해도 이중 곱셈 없이 캐릭터 축소/확대 비율을 그대로 반영한다.
	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetRelativeScale3D(FVector(HealthWidgetBaseScale * Scale));
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

void AFTPlayerCharacterBase::OnHealthWidgetDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	//	태그가 제거된 순간(NewCount == 0)만 처리 — 부여될 때(사망)는 위젯 쪽에서 이미 처리한다.
	if (NewCount > 0)
	{
		return;
	}

	//	Revive()는 서버에서만 실행되므로(위 Dead 태그 주석과 동일 사유), 사망 포즈로 홀드돼 있던 DeathMontage를
	//	직접 여기서(복제되는 Dead 태그가 해제되는 시점에) 각 클라이언트가 로컬로 풀어준다 — 그래야 소유 클라이언트
	//	자신의 화면에서도 기상 연출이 보인다. (DeathMontage는 bEnableAutoBlendOut=false로 마지막 포즈를 유지한다.)
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimMontage* Death = GetDeathMontage())
		{
			AnimInstance->Montage_Stop(0.25f, Death);
		}
	}

	if (!HealthWidgetComponent)
	{
		return;
	}

	if (UUserWidget* Widget = HealthWidgetComponent->GetUserWidgetObject())
	{
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void AFTPlayerCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	const bool bIsLocal = IsLocallyControlled();
	CameraBoom->SetActive(bIsLocal);
	FollowCamera->SetActive(bIsLocal);

	// 내 캐릭터는 HUD에 체력이 이미 표시되므로, 머리 위 체력바는 다른 플레이어 캐릭터에만 보이도록 숨긴다.
	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetVisibility(!bIsLocal, true);
	}
}

void AFTPlayerCharacterBase::OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = Data.NewValue;
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

			// BeginPlay 시점엔 아직 PlayerState가 붙기 전이라 HealthWidgetComponent의 자체 ASC 바인딩이
			// 실패해 있으므로, Possess가 확정된 지금 다시 바인딩한다.
			if (HealthWidgetComponent)
			{
				HealthWidgetComponent->InitializeWithAbilitySystem(ASC);
			}

			// Dead 태그 해제(부활) 이벤트 구독 — 서버에서 등록되지만, 태그 자체가 복제되므로
			// 콜백은 이 태그를 보는 모든 클라이언트에서 각자 로컬로 호출된다.
			if (HealthWidgetDeadTagEventHandle.IsValid())
			{
				ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved).Remove(HealthWidgetDeadTagEventHandle);
			}
			HealthWidgetDeadTagEventHandle = ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AFTPlayerCharacterBase::OnHealthWidgetDeadTagChanged);

			// 이동 속도(MoveSpeed) 변경 이벤트 구독 — 버프 부여/만료 및 기본값 변경을 가장 완벽하게 감지합니다.
			if (MoveSpeedChangeEventHandle.IsValid())
			{
				ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMoveSpeedAttribute()).Remove(MoveSpeedChangeEventHandle);
			}
			MoveSpeedChangeEventHandle = ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMoveSpeedAttribute())
				.AddUObject(this, &AFTPlayerCharacterBase::OnMoveSpeedAttributeChanged);

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

				// 캐릭터 행(FFTCharacterData)의 4개 버튼 슬롯에서 스킬을 부여.
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

			// 클라이언트에서도 동일하게 PlayerState 복제 완료 시점에 재바인딩한다.
			if (HealthWidgetComponent)
			{
				HealthWidgetComponent->InitializeWithAbilitySystem(ASC);
			}

			// PossessedBy는 서버에서만 호출되므로, 각 클라이언트에서도 여기서 동일하게 구독해야
			// 원격 클라이언트 화면에서도 부활 시 체력바가 다시 보인다.
			if (HealthWidgetDeadTagEventHandle.IsValid())
			{
				ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved).Remove(HealthWidgetDeadTagEventHandle);
			}
			HealthWidgetDeadTagEventHandle = ASC->RegisterGameplayTagEvent(FTTags::FTStates::Core::Dead, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &AFTPlayerCharacterBase::OnHealthWidgetDeadTagChanged);

			if (MoveSpeedChangeEventHandle.IsValid())
			{
				ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMoveSpeedAttribute()).Remove(MoveSpeedChangeEventHandle);
			}
			MoveSpeedChangeEventHandle = ASC->GetGameplayAttributeValueChangeDelegate(UFT_AttributeSet::GetMoveSpeedAttribute())
				.AddUObject(this, &AFTPlayerCharacterBase::OnMoveSpeedAttributeChanged);
		}
	}
}

void AFTPlayerCharacterBase::InitializeCharacterAttribute() const
{
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
