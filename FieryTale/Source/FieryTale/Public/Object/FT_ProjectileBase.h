// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Actor.h"
#include "FT_ProjectileBase.generated.h"

// 💡 [컴파일 가드 전방 선언 명세 추가 완착]
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent; // 스태틱 메시 전방 선언 누락 균열 보수 완료

UCLASS()
class FIERYTALE_API AFT_ProjectileBase : public AActor
{
    GENERATED_BODY()
    
public: 
    AFT_ProjectileBase();

    // 공격을 가한 GA 측에서 발사 직전 이 투사체에 최종 데미지 계산서를 주입해 주는 통로입니다.
    UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "FieryTale | GAS")
    FGameplayEffectSpecHandle DamageEffectSpecHandle;

    /** 적중 시 추가로 적용할 상태 이상(슬로우, 기절 등) 이펙트 스펙들입니다. */
    UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "FieryTale | GAS")
    TArray<FGameplayEffectSpecHandle> AdditionalEffectSpecHandles;

protected:
    virtual void BeginPlay() override;

    // 💡 [언리얼 순정 수명 주기 가상 함수 오버라이드 등록]
    // 서버가 Destroy를 집행하면 모든 클라이언트 프록시 단에서도 이 함수가 격발되어 렉 잔상을 소각합니다.
    virtual void Destroyed() override;

    // 순정 오버랩 관문으로 복귀합니다. 적중 즉시 대미지를 주고 소멸합니다.
    UFUNCTION()
    virtual void OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

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
    /** 물리 기동 컴포넌트와 콜리전을 서버에서 즉각 철거 요청하는 마감 함수 */
    void ExplodeAndDestroy();

    // 💡 [수명 주기 연출 분리용 플래그 장부 등록 완착]
    // 허공에서 수명이 다해 소멸한 것과 적/벽에 충돌해 터진 상태를 정밀 필터링합니다.
    UPROPERTY(Transient)
    uint8 bExploded : 1;
};