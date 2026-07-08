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

    // 공격을 가한 GA 측에서 발사 직전 이 투사체에 최종 데미지 계산서를 주입해 주는 통로입니다.
    UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "FieryTale | GAS")
    FGameplayEffectSpecHandle DamageEffectSpecHandle;

protected:
    virtual void BeginPlay() override;

    // 순정 오버랩 관문으로 복귀합니다. 적중 즉시 대미지를 주고 소멸합니다.
    UFUNCTION()
    virtual void OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // =========================================================================
    // 💡 [지형/벽면 충돌 및 소각 파쇄 선로 추가 완착]
    // =========================================================================
    /** 지형, 벽면, 포탑 구조물 등 블록 콜리전 히트를 정밀 수신할 순정 델리게이트 함수 */
    UFUNCTION()
    virtual void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    // [코어 컴포넌트 슬롯]
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
    TObjectPtr<USphereComponent> SphereComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
    // =========================================================================
    // 💡 [클라이언트 유령 패킷 안락사 파이프라인]
    // =========================================================================
    /** 물리 기동 컴포넌트와 콜리전을 서버/클라이언트 양단에서 즉각 동결 소각 처리하는 마감 함수 */
    void ExplodeAndDestroy();
};