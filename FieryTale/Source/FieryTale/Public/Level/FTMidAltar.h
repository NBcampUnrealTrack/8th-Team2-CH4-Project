#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "FTMidAltar.generated.h"

UENUM(BlueprintType)
enum class EFTAltarTeam : uint8
{
    None = 0 UMETA(DisplayName = "Neutral"), // 미점령 기본 상태
    BlueTeam = 1 UMETA(DisplayName = "Blue Team"), // 블루팀 상태
    RedTeam = 2 UMETA(DisplayName = "Red Team") // 레드팀 상태
};

UCLASS()
class FIERYTALE_API AFTMidAltar : public AActor, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    AFTMidAltar();
    
    virtual void Tick(float DeltaTime) override;
    
    virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    
    virtual void SetGenericTeamId(const FGenericTeamId& InTeamID) override;
    
    virtual FGenericTeamId GetGenericTeamId() const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> AltarMesh; // 제단 외형 스태틱 메시 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UBoxComponent> CaptureVolume; // 점령 영역 감지용 콜리전 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent; // GAS 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AttributeSet> AttributeSet; // 체력 및 점령 속성 세트

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    EFTAltarTeam AltarTeam; // 제단 소속 진영 변수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName TeamColorParameterName; // 머티리얼 벡터 파라미터 이름

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor NeutralColor; // 미점령 상태 기본 색상

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor BlueTeamColor; // 블루팀 적용 색상

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor RedTeamColor; // 레드팀 적용 색상

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Debug")
    bool bEnableSimulation; // 자동 비주얼 테스트 시뮬레이션 활성화 플래그

    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult); // 구역 진입 감지 함수

private:
    FGenericTeamId TeamId; // 피아식별용 팀 ID
    
    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> DynamicAltarMaterial; // 동적 제어용 머티리얼 인스턴스
    
    float SimElapsedTime; // 시뮬레이션 경과 시간 누적 변수

    void UpdateAltarColor(FLinearColor NewColor); // 머티리얼 파라미터 색상 갱신 함수
    
    void HandleSimulation(float DeltaTime); // 시뮬레이션 타임라인 처리 함수
};