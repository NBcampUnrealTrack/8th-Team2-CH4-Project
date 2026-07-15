#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FTBush.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class APawn;

UCLASS()
class FIERYTALE_API AFTBush : public AActor
{
    GENERATED_BODY()

public:
    AFTBush();
    
    // 네트워크 리플리케이션 속성 등록
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 대상을 1초간 부쉬 안에서 노출시키는 함수
    UFUNCTION(BlueprintCallable, Category = "FT|Bush")
    void RevealOccupant(APawn* Occupant);

protected:
    // 게임 시작 시 초기화 수행
    virtual void BeginPlay() override;

    // 투사체 관통을 위해 콜리전이 해제된 시각적 부쉬 메쉬
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BushMesh;

    // 폰 전용 겹침 감지 트리거 볼륨
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> OverlapVolume;

    // 투명도 조절용 머티리얼 파라미터 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FT|Material")
    FName OpacityParameterName;

    // 폰 진입 감지 이벤트
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 폰 이탈 감지 이벤트
    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    // 현재 부쉬에 들어와 있는 진입자 배열
    UPROPERTY(ReplicatedUsing = OnRep_Occupants)
    TArray<TObjectPtr<APawn>> Occupants;

    // 스킬 사용 등으로 발각된 진입자 배열
    UPROPERTY(ReplicatedUsing = OnRep_RevealedOccupants)
    TArray<TObjectPtr<APawn>> RevealedOccupants;

    // Occupants 배열 갱신 시 가시성 동기화
    UFUNCTION()
    void OnRep_Occupants(const TArray<APawn*>& OldOccupants);

    // RevealedOccupants 배열 갱신 시 가시성 동기화
    UFUNCTION()
    void OnRep_RevealedOccupants();

    // 부쉬 내 팀 기반 시야 공유 및 노출 상태 업데이트
    void UpdateBushEffects();

    // 런타임 투명도 제어용 다이내믹 머티리얼 인스턴스
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

    // 대상별 1초 발각 상태 타이머 관리 컨테이너
    TMap<APawn*, FTimerHandle> RevealTimers;

    // 1초 타이머 만료 시 은신 상태 복구
    void RestoreInvisibility(APawn* Occupant);

    // 대상의 ASC에 은신 상태이상 태그 부여 및 회수
    void SetInvisibilityTag(APawn* Occupant, bool bAddTag);
};