#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Character/FTCharacterTypes.h"
#include "FTMidAltar.generated.h"

UENUM(BlueprintType)
enum class EFTAltarTeam : uint8
{
    None = 0 UMETA(DisplayName = "Neutral"),
    BlueTeam = 1 UMETA(DisplayName = "Blue Team"),
    RedTeam = 2 UMETA(DisplayName = "Red Team")
};

UENUM(BlueprintType)
enum class EFTAltarState : uint8
{
    Neutral,
    Capturing,
    Decaying,
    Contested,
    Captured,
    Locked
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
    
    // 제단을 중립 상태로 초기화
    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "FieryTale|Altar")
    void OpenAltar();
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> AltarMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> HologramRingMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class USphereComponent> CaptureVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UAudioComponent> CaptureAudioComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AttributeSet> AttributeSet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName TeamColorParameterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName ProgressParameterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName ContestedParameterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor NeutralColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor BlueTeamColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor RedTeamColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    float MaxCaptureTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    float CaptureDecayMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    float LockDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|GAS")
    TSubclassOf<class UGameplayEffect> CaptureBuffClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Audio")
    TObjectPtr<class USoundBase> CaptureLoopSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Audio")
    TObjectPtr<class USoundBase> CaptureSuccessSound;

    // 캡처 구역 진입 감지
    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 캡처 구역 이탈 감지
    UFUNCTION()
    void OnOverlapEnd(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    FGenericTeamId TeamId;
    
    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> DynamicAltarMaterial;
    
    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> DynamicHologramMaterial;
    
    UPROPERTY()
    TArray<TObjectPtr<class AActor>> OverlappingPlayers;
    
    // 메쉬 및 오디오 시각화 업데이트
    void UpdateAltarVisuals();
    
    // 점령 진행 규칙 서버 연산
    void ProcessCaptureRules(float DeltaTime);
    
    // 액터의 소속 팀 식별
    EFTTeam GetCharacterTeam(class AActor* Actor, bool& bIsValid) const;
    
    // 캡처 데이터 리플리케이션 처리
    UFUNCTION()
    void OnRep_CaptureData();
    
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    EFTAltarState CurrentState;
    
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    EFTAltarTeam AltarTeam;
    
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    EFTAltarTeam CurrentControllingTeam;
    
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    float CaptureProgress;
    
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    float RemainingLockTime;

    EFTAltarState PreviousState;
};