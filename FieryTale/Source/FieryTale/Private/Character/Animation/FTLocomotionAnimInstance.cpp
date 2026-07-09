// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Animation/FTLocomotionAnimInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"

void UFTLocomotionAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	//	소유 폰과 이동 컴포넌트를 1회 확보해둔다.
	OwningPawn = TryGetPawnOwner();
	if (OwningPawn)
	{
		MovementComponent = Cast<UCharacterMovementComponent>(OwningPawn->GetMovementComponent());
	}
}

void UFTLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	//	AnimBP 프리뷰/스폰 타이밍 등으로 초기화 시점에 폰이 없었을 수 있으니 한 번 더 지연 확보한다.
	if (!OwningPawn)
	{
		OwningPawn = TryGetPawnOwner();
		if (!OwningPawn)
		{
			return;
		}
		MovementComponent = Cast<UCharacterMovementComponent>(OwningPawn->GetMovementComponent());
	}

	//	속도는 CharacterMovementComponent에서 복제되므로 원격 클라이언트(Simulated Proxy)에서도 유효하다.
	const FVector Velocity = OwningPawn->GetVelocity();

	//	수평 속력만 사용한다(스트레이프라 Z는 무의미하고, 점프도 없음).
	GroundSpeed = Velocity.Size2D();

	//	몸 정면 기준 이동 방향 각도 → 2D 블렌드스페이스 가로축.
	Direction = CalculateDirection(Velocity, OwningPawn->GetActorRotation());

	//	실제 가속 입력이 있고(관성으로 미끄러지는 중이 아님) 속력이 임계값을 넘을 때만 이동으로 판정.
	//	이동 컴포넌트가 없으면(비정상) 속력만으로 대체 판정한다.
	const bool bHasAccelInput = MovementComponent
		? !MovementComponent->GetCurrentAcceleration().IsNearlyZero()
		: (GroundSpeed > MoveSpeedThreshold);
	bShouldMove = bHasAccelInput && (GroundSpeed > MoveSpeedThreshold);
}

float UFTLocomotionAnimInstance::CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation)
{
	if (Velocity.IsNearlyZero())
	{
		return 0.f;
	}

	//	기준 회전(몸 정면)을 축으로 이동 벡터가 좌/우로 얼마나 벌어졌는지를 각도로 환산한다.
	const FMatrix RotMatrix = FRotationMatrix(BaseRotation);
	const FVector ForwardVector = RotMatrix.GetScaledAxis(EAxis::X);
	const FVector RightVector = RotMatrix.GetScaledAxis(EAxis::Y);
	const FVector NormalizedVel = Velocity.GetSafeNormal2D();

	//	정면과의 사잇각(0~180)을 구하고, 우측 성분의 부호로 좌(-)/우(+)를 결정한다.
	//	부동소수 오차로 Dot이 [-1,1]을 살짝 벗어나 Acos가 NaN이 되는 것을 Clamp로 방지한다.
	const float ForwardCosAngle = FMath::Clamp(FVector::DotProduct(ForwardVector, NormalizedVel), -1.f, 1.f);
	const float ForwardDeltaDegree = FMath::RadiansToDegrees(FMath::Acos(ForwardCosAngle));
	const float RightCosAngle = FVector::DotProduct(RightVector, NormalizedVel);

	return (RightCosAngle < 0.f) ? -ForwardDeltaDegree : ForwardDeltaDegree;
}
