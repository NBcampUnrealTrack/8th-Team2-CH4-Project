// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FTLocomotionAnimInstance.generated.h"

class APawn;
class UCharacterMovementComponent;

/**
 *	FieryTale 캐릭터 공용 로코모션 AnimInstance 베이스.
 *
 *	"이동 애니메이션에 필요한 값만" 계산해 BlueprintReadOnly로 노출한다.
 *	캐릭터별 AnimBP(ABP_RedHood 등)는 이 클래스를 상속하고, AnimGraph에서
 *	여기 값(Direction / GroundSpeed)을 2D 블렌드스페이스에 꽂기만 하면 된다.
 *	→ 계산 로직은 이 C++ 한 곳에, 에셋(블렌드스페이스/몽타주)만 캐릭터별 BP에.
 *
 *	설계 전제:
 *	- 스트레이프 이동이다(bUseControllerRotationYaw=true, bOrientRotationToMovement=false).
 *	  바라보는 방향과 무관하게 앞/뒤/좌/우로 움직이므로, 속도 1D가 아니라
 *	  Direction(-180~180) + GroundSpeed 2D 블렌드스페이스가 필요하다.
 *	- 점프를 사용하지 않으므로 공중/낙하 관련 상태·변수는 두지 않는다.
 *	- AnimInstance는 폰 상태를 "읽기"만 한다. 게임플레이(대미지/상태)를 여기서 바꾸지 말 것.
 *	  이동값은 CharacterMovementComponent의 복제된 Velocity에서 파생되므로,
 *	  원격 클라이언트(Simulated Proxy)도 별도 처리 없이 자동으로 맞는다.
 *	- 스킬/피격/사망 등 나머지 애니는 이 클래스가 관여하지 않는다. AnimGraph 말단의
 *	  Slot 'DefaultSlot' 노드로 몽타주(어빌리티 PlayMontageAndWait / GameplayCue)가 덮어쓴다.
 */
UCLASS()
class FIERYTALE_API UFTLocomotionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	//	수평 이동 속력(cm/s). 2D 블렌드스페이스의 세로축(속도) 및 Idle↔Move 판정에 사용.
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed = 0.f;

	//	몸(액터) 정면 기준 이동 방향 각도(-180~180). 2D 블렌드스페이스의 가로축.
	//	0=전진, ±180=후진, +90=우측 스트레이프, -90=좌측 스트레이프.
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;

	//	이동 애니로 전환할지 여부(가속 입력이 있고 실제로 움직이는 중). Idle↔Move 전이 조건.
	//	블렌드스페이스만 쓸 거면 무시해도 되지만, 관성 미끄러짐에 정지 애니를 물리려면 이 값을 전이에 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bShouldMove = false;

	//	bShouldMove / 정지 판정용 최소 속력 임계값. 이 값 이하이면 Idle로 본다.
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion")
	float MoveSpeedThreshold = 3.f;

private:
	//	소유 폰과 이동 컴포넌트 캐시(NativeInitializeAnimation에서 1회 확보 — 매 프레임 조회 방지).
	UPROPERTY(Transient)
	TObjectPtr<APawn> OwningPawn;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	//	속도 벡터와 기준 회전으로 -180~180 방향 각도를 구한다.
	//	UKismetAnimationLibrary::CalculateDirection과 동일 로직을, AnimGraphRuntime 모듈
	//	의존성을 추가하지 않기 위해 내장 구현한다.
	static float CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation);
};
