#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "FTTurret.generated.h"

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8
{
    None = 0 UMETA(Hidden), // UHT 컴파일 에러 방지용 기본값 지정
    BlueTeam = 1 UMETA(DisplayName = "Blue Team"), // 블루팀 지정
    RedTeam = 2 UMETA(DisplayName = "Red Team") // 레드팀 지정
};

UCLASS()
class FIERYTALE_API AFTTurret : public APawn, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    AFTTurret();
    
    virtual void Tick(float DeltaTime) override;
    
    virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    
    virtual void SetGenericTeamId(const FGenericTeamId& InTeamID) override;
    
    virtual FGenericTeamId GetGenericTeamId() const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> TurretMesh; // 포탑 외형을 담당하는 스태틱 메시 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent; // 포탑 전용 자체 GAS 컴포넌트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AttributeSet> AttributeSet; // 포탑의 체력 데이터를 담을 어트리뷰트 셋

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Turret")
    EFTTurretTeam TurretTeam; // 에디터에서 포탑의 소속 진영을 결정하는 변수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Effects")
    TObjectPtr<class USoundBase> DestructionSound; // 파괴 시 재생할 오디오 에셋

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Effects")
    TObjectPtr<class UParticleSystem> DestructionEffect; // 파괴 시 스폰할 파티클 에셋

    void OnHealthChanged(const struct FOnAttributeChangeData& Data); // 체력 변화 시 호출될 델리게이트 함수

    UFUNCTION()
    void DebugDestroyTurret(); // 10초 후 타이머에 의해 호출될 디버그용 파괴 함수

private:
    FTimerHandle AutoDestroyTimerHandle; // 10초 자동 파괴를 예약 및 관리할 타이머 핸들

    FGenericTeamId TeamId; // 피아식별 인터페이스용 진영 ID 캐싱 변수
    
    bool bIsDying; // 파괴 연출이 진행 중인지 확인하는 플래그
    
    float DyingTimer; // 파괴 연출 진행 시간을 추적하는 타이머
    
    float MaxDyingTime; // 파괴 하강 연출이 지속될 총 시간
    
    FVector InitialLocation; // 하강 연출 시작 전 포탑의 원본 위치 캐싱
};