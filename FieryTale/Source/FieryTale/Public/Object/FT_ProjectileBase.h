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

    // [코어 컴포넌트 슬롯]
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
    TObjectPtr<USphereComponent> SphereComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Component")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
};