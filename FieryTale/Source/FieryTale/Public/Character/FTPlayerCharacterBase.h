// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/FTCharacterBase.h"
#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "FTPlayerCharacterBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UTimelineComponent;
class UCurveFloat;
class UFTHealthWidgetComponent;
struct FInputActionValue;
struct FFTCharacterData;

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

	//	머리 위 체력바 — PostInitializeComponents에서 C++이 직접 만들어 부착한다(BP 컴포넌트 트리에 손대지 않음).
	UPROPERTY(BlueprintReadOnly, Category = UI, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFTHealthWidgetComponent> HealthWidgetComponent;

	//	HealthWidgetComponent에 실제로 표시할 위젯 블루프린트. BP_FTPlayerCharacterBase의 Class Defaults에서
	//	지정한다 — 컴포넌트 트리가 아니라 단순 프로퍼티라 이름 충돌/고아 오버라이드 문제가 없다.
	UPROPERTY(EditDefaultsOnly, Category = UI)
	TSoftClassPtr<UUserWidget> HealthWidgetClass;

	// TODO:: GetWeaponData시 CharacterData->GetWeaponData(); 따로 필드에 이 값을 갖고 있을 필요성이 있는가?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFT_WeaponData> CurrentWeaponData;

	//	FOV 연출용 타임라인 — 액터 Tick과 무관하게 자체적으로 틱하는 컴포넌트라 PrimaryActorTick 설정을 건드릴 필요가 없다.
	UPROPERTY()
	TObjectPtr<UTimelineComponent> FovTimeline;

	//	커브가 지정되지 않았을 때 쓰는 0→1 선형 커브 (런타임 생성)
	UPROPERTY()
	TObjectPtr<UCurveFloat> DefaultLinearFovCurve;

	//	평타/스킬별 다중 포인트 펀치 커브 캐시 — 최초 호출 시 1회만 생성해 재사용한다.
	UPROPERTY()
	TObjectPtr<UCurveFloat> AttackFovCurve;

	UPROPERTY()
	TObjectPtr<UCurveFloat> UltimateFovCurve;

	UPROPERTY()
	TObjectPtr<UCurveFloat> UtilFovCurve;

	//	카메라 연출의 기준/복귀 목표 FOV — 생성자에서 FollowCamera 초기값으로 설정된다.
	float DefaultFov = 90.f;

	//	CameraBoom의 최초 SocketOffset — BeginPlay에서 캐싱해두고, ResetCameraLocation의 복귀 목표로 쓴다.
	FVector DefaultCameraSocketOffset = FVector::ZeroVector;

	//	CameraBoom의 최초 TargetArmLength — BeginPlay에서 캐싱해두고, SetCameraDistanceScale의 배율 기준값으로 쓴다.
	//	SocketOffset은 카메라 거리에서 차지하는 비중이 작아(기본 450 vs 오프셋 수십), 캐릭터 축소에 맞춰 카메라를
	//	"똑같은 상대 거리"로 당기려면 실제 거리를 결정하는 ArmLength 자체를 배율로 조절해야 한다.
	float DefaultCameraArmLength = 450.f;

	//	체력바 위젯의 기본 축소 배율(모델 원본 크기 기준) — PostInitializeComponents에서 HealthWidgetComponent의
	//	RelativeScale3D로 적용된다. SetCharacterScale(모델 Scale)과 곱해져 항상 "기본 배율 × 현재 모델 배율"로
	//	재계산되므로, 반복 축소/확대에도 값이 누적되지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = UI)
	float HealthWidgetBaseScale = 0.25f;

	//	캡슐 상단 기준 체력바 추가 높이 오프셋.
	UPROPERTY(EditDefaultsOnly, Category = UI)
	float HealthWidgetZOffset = 30.f;

	//	캡슐/메시 축소·확대(SetCharacterScale) 연출의 기준값 — ApplyCharacterVisuals에서 메시를 확정 배치한 직후 캐싱된다.
	//	모든 스케일 변경은 이 기본값에 배율을 곱하는 절대 계산 방식이라, 중첩 호출되어도 누적 오차 없이 항상 같은 결과로 복원된다.
	float DefaultCapsuleRadius = 42.f;
	float DefaultCapsuleHalfHeight = 96.f;
	FVector DefaultMeshRelativeScale = FVector::OneVector;
	FVector DefaultMeshRelativeLocation = FVector::ZeroVector;

	//	실제 축소/확대 배율의 서버 권위 값 — 서버의 SetCharacterScale이 갱신하면 모든 클라이언트로 복제되어
	//	OnRep_CharacterScale이 캡슐/메시를 동일하게 맞춘다 (제3자 클라이언트도 같은 크기를 보게 하기 위함).
	UPROPERTY(ReplicatedUsing = OnRep_CharacterScale)
	float ReplicatedCharacterScale = 1.f;

	UFUNCTION()
	void OnRep_CharacterScale();

	//	실제 캡슐 크기/메시 스케일·위치를 갱신하는 로컬 적용부 — 서버의 SetCharacterScale과
	//	각 클라이언트의 OnRep_CharacterScale이 공통으로 호출한다.
	void ApplyCharacterScaleVisual(float Scale);

	float FovEffectStartValue = 90.f;
	float FovEffectTargetValue = 90.f;
	bool bFovTrackInitialized = false;

	//	PlayCameraFovPunch가 예약하는 자동 복귀 호출용 타이머
	FTimerHandle FovResetTimerHandle;

	UFUNCTION()
	void OnFovTimelineUpdate(float Alpha);

	//	Keys(정규화된 0~1 시간, 알파값)로 이루어진 다중 포인트 커브를 CurveCache에 없으면 만들고, 있으면 재사용해 반환한다.
	UCurveFloat* GetOrBuildFovCurve(TObjectPtr<UCurveFloat>& CurveCache, const TArray<FVector2D>& Keys);

	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

public:
	AFTPlayerCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;

	// ASC는 PlayerState에서 가져옴
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// [이관/폐기 보존] 구: UFT_CharacterData 포인터를 직접 참조하던 방식.
	//UPROPERTY(ReplicatedUsing = OnRep_CharacterData, EditAnywhere, BlueprintReadWrite, Category = "FieryTale | Character")
	//TObjectPtr<UFT_CharacterData> CharacterData;
	//
	//UFUNCTION()
	//void OnRep_CharacterData();

	// 신: DT_CharacterData(FFTCharacterData) 행을 가리키는 핸들. BP 인스턴스 또는 스폰 시 주입으로 지정한다.
	UPROPERTY(ReplicatedUsing = OnRep_CharacterRow, EditAnywhere, BlueprintReadWrite, Category = "FieryTale | Character", meta = (RowType = "FTCharacterData"))
	FDataTableRowHandle CharacterRow;

	UFUNCTION()
	void OnRep_CharacterRow();

	//	CharacterRow를 FFTCharacterData*로 해석한다. 테이블/행이 유효하지 않으면 nullptr.
	const FFTCharacterData* GetCharacterData() const;

	UFUNCTION(BlueprintCallable, Category = "FieryTale | Weapon")
	UFT_WeaponData* GetWeaponData() const;

	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	//	FollowCamera의 FOV를 Duration초에 걸쳐 TargetFov로 보간하는 연출. Curve를 안 주면 선형으로 보간한다.
	//	스킬(GameplayAbility) 등 외부에서 카메라 연출을 걸고 싶을 때 호출한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void PlayCameraFovEffect(float TargetFov, float Duration, UCurveFloat* Curve = nullptr);

	//	PlayCameraFovEffect로 TargetFov까지 펀치인한 뒤, PunchDuration+HoldDuration초 후 자동으로
	//	ResetCameraFov를 호출해 기본 FOV로 되돌린다. 평타/스킬처럼 "확 당겼다 원복"되는 연출에 사용.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void PlayCameraFovPunch(float TargetFov, float PunchDuration, float HoldDuration, float ReturnDuration, UCurveFloat* PunchCurve = nullptr);

	//	FollowCamera의 FOV를 기본값(DefaultFov)으로 되돌리는 연출 — 모든 FOV 원복은 이 함수 하나로 통일한다.
	//	Duration이 0 이하면 즉시(스냅) 적용하고 진행 중이던 연출/예약된 자동 복귀는 취소한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void ResetCameraFov(float Duration = 0.2f);

	//	CameraBoom의 SocketOffset을 외부에서 넘겨준 위치로 즉시 변경한다. (예: 캐릭터 크기 축소 시 줌인 연출)
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void SetCameraLocation(const FVector& NewLocation);

	//	CameraBoom의 SocketOffset을 BeginPlay 시점에 저장해둔 최초 위치(DefaultCameraSocketOffset)로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void ResetCameraLocation();

	//	CameraBoom의 TargetArmLength를 "최초 길이 * Scale"로 조절한다. 캐릭터가 Scale배로 작아질 때 함께 호출하면
	//	카메라도 같은 비율로 가까워져, 화면상 상대 거리(체감 크기)가 축소 전과 동일하게 유지된다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void SetCameraDistanceScale(float Scale);

	//	SetCameraDistanceScale(1.f)와 동일 — CameraBoom의 TargetArmLength를 최초 길이로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Camera")
	void ResetCameraDistanceScale();

	//	캡슐 콜리전과 메시를 DefaultCapsuleRadius/HalfHeight, DefaultMeshRelativeScale/Location 기준으로
	//	Scale배 만큼 동시에 축소/확대한다. 캡슐은 항상 SetCapsuleSize로만 조절하고 액터 자체는 스케일하지 않아,
	//	루트(캡슐)에 스케일이 이중으로 곱해져 메시가 바닥 아래로 파고드는 문제를 원천적으로 방지한다.
	//	서버 권한에서 호출 시 ReplicatedCharacterScale을 갱신해 모든 클라이언트(제3자 포함)에 복제 전파하고,
	//	호출한 로컬(서버 자신 혹은 예측 실행 중인 오너 클라이언트)에는 즉시 시각 반영한다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Character")
	void SetCharacterScale(float Scale);

	//	SetCharacterScale(1.f)와 동일 — 캡슐/메시를 기본 크기로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "FieryTale | Character")
	void ResetCharacterScale();

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

	//	Dead 태그는 복제되는 GameplayTag라 각 클라이언트에서 로컬로 이 콜백이 호출된다.
	//	Revive()는 서버에서만 실행되므로(ExecuteRespawn이 HasAuthority 체크), 원격 클라이언트에서도
	//	체력바가 다시 보이게 하려면 여기서(태그 해제 시점) 처리해야 한다.
	void OnHealthWidgetDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	//	위 델리게이트의 등록/해제를 핸들로 명시 관리한다(RemoveAll(this) 대신). PossessedBy/OnRep_PlayerState가
	//	여러 번 불릴 수 있어, 재등록 전 이 핸들로만 정확히 이전 바인딩을 제거해 중복 등록을 막는다.
	FDelegateHandle HealthWidgetDeadTagEventHandle;
};
