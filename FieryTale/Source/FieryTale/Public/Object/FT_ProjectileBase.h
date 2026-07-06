// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Actor.h"
#include "FT_ProjectileBase.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

UCLASS()
class FIERYTALE_API AFT_ProjectileBase : public AActor
{
	GENERATED_BODY()
    
public:	
	AFT_ProjectileBase();

	/** ◄◄◄ [GAS 투사체 핵심 배관 슬롯]
	 * 공격을 가한 GA 측에서 발사 직전 이 투사체에 최종 데미지 계산서(Spec)를 주입해 주는 통로입니다.
	 */
	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "FieryTale | GAS")
	FGameplayEffectSpecHandle DamageEffectSpecHandle;

protected:
	virtual void BeginPlay() override;

	/** 충돌 시 적중 대상을 판별하고 데미지 배관을 연결할 관문 */
	UFUNCTION()
	virtual void OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// =========================================================================
	// [코어 컴포넌트 슬롯]
	// =========================================================================
	/** 투사체의 실시간 피격 판정을 전담할 구체 충돌 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
	TObjectPtr<USphereComponent> SphereComponent;

	/** 투사체의 비주얼을 담당할 메시 컴포넌트 (에디터에서 자유롭게 교체 가능) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	/** 투사체의 속도, 중력, 유도(Homing) 전반을 제어하는 엔진 순정 물리 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
};
