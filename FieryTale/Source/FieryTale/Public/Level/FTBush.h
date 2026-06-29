#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FTBush.generated.h"

UCLASS()
class FIERYTALE_API AFTBush : public AActor
{
    GENERATED_BODY()

public:
    AFTBush();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> BushMesh; // 스태틱 메시 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UBoxComponent> OverlapVolume; // 콜리전 컴포넌트

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FT|Material")
    FName OpacityParameterName; // 머티리얼 투명도 파라미터 이름

    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult); // 진입 감지 이벤트

    UFUNCTION()
    void OnOverlapEnd(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex); // 이탈 감지 이벤트

private:
    UPROPERTY()
    TArray<TObjectPtr<class APawn>> Occupants; // 테스트를 위해 배열 타입을 APawn으로 변경

    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> DynamicMaterial; // 런타임 제어용 다이내믹 머티리얼

    void UpdateBushEffects(); // 가시성 및 머티리얼 투명도 갱신 함수
};