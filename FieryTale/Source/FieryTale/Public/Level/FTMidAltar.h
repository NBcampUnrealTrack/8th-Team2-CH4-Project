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
    None = 0 UMETA(DisplayName = "Neutral"), // 미점령 기본 상태
    BlueTeam = 1 UMETA(DisplayName = "Blue Team"), // 블루팀 상태
    RedTeam = 2 UMETA(DisplayName = "Red Team") // 레드팀 상태
};

UENUM(BlueprintType)
enum class EFTAltarState : uint8
{
    Neutral, // 아무도 점령하지 않은 상태
    Progressing, // 한 팀에 의해 점령이 가산 혹은 감산되는 상태
    Contested, // 양 팀 플레이어가 동시 진입하여 대치 중인 상태
    Captured, // 점령이 완전히 완료되어 소유권이 확정된 상태
    Locked // 점령 완료 후 지정 시간 동안 비활성화된 잠금 상태
};

UCLASS()
class FIERYTALE_API AFTMidAltar : public AActor, public IAbilitySystemInterface, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    AFTMidAltar();
    virtual void Tick(float DeltaTime) override;
    
    // GAS 컴포넌트를 외부로 제공하는 인터페이스 함수입니다.
    virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    
    // 제단의 기본 소속 팀 ID를 강제 변경하는 함수입니다.
    virtual void SetGenericTeamId(const FGenericTeamId& InTeamID) override;
    
    // 제단에 부여된 고유 팀 ID를 반환하는 함수입니다.
    virtual FGenericTeamId GetGenericTeamId() const override;

    // 거점 점령 상태를 완전히 초기화하고 재개방하는 함수입니다.
    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "FieryTale|Altar")
    void OpenAltar();

    // 네트워크 복제 타겟 변수들을 전송 목록에 등록합니다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // 다이내믹 머티리얼 인스턴스 할당 및 대리자 바인딩을 수행합니다.
    virtual void BeginPlay() override;

    // 제단 바닥부 외형을 표현하는 스태틱 메시 컴포넌트입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> AltarMesh;

    // 허공에 생성되는 통과 가능한 반투명 링 실린더 컴포넌트입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class UStaticMeshComponent> HologramRingMesh;

    // 원형 점령 구역 진입 플레이어를 감지하기 위한 구체 콜리전 컴포넌트입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<class USphereComponent> CaptureVolume;

    // 제단 내부에서 작동할 어빌리티 시스템 컴포넌트입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AbilitySystemComponent> AbilitySystemComponent;

    // 내구도 및 관련 특성을 보관하는 속성 세트입니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<class UFT_AttributeSet> AttributeSet;

    // 에디터에서 보정할 머티리얼 벡터 컬러 파라미터 명칭입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName TeamColorParameterName;

    // 에디터에서 매핑할 머티리얼 진행률 스칼라 파라미터 명칭입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName ProgressParameterName;

    // 대치(Contested) 중 점멸 상태를 제어할 스칼라 파라미터 명칭입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FName ContestedParameterName;

    // 점령 중립 시 테두리에 뿌려질 기본 회색 컬러 정보입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor NeutralColor;

    // 블루팀 점령 시 적용되는 블루 컬러 정보입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor BlueTeamColor;

    // 레드팀 점령 시 적용되는 레드 컬러 정보입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Material")
    FLinearColor RedTeamColor;

    // 거점 점령에 소요되는 총 요구 목표 시간 상수입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    float MaxCaptureTime;

    // 역점령을 시도하여 기존 점령 게이지를 깎아내릴 때의 가속 배율입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    float CaptureDecayMultiplier;

    // 점령 완료 시 거점이 영구 잠금 상태로 유지되는 총 시간입니다. (초 단위, 기본값 60초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|Altar")
    float LockDuration;

    // 거점 점령 완료 시 승리 팀 구역 내 플레이어들에게 부여할 GAS 버프 클래스입니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieryTale|GAS")
    TSubclassOf<class UGameplayEffect> CaptureBuffClass;

    // 범위 진입 시 작동할 다이내믹 바인딩 이벤트 함수입니다.
    UFUNCTION()
    void OnOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // 범위 이탈 시 목록에서 플레이어를 소거할 다이내믹 바인딩 이벤트 함수입니다.
    UFUNCTION()
    void OnOverlapEnd(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    // 제단에 세팅된 내부 피아식별용 ID 구조체입니다.
    FGenericTeamId TeamId;
    
    // 바닥 스태틱 메시의 런타임 다이내믹 머티리얼입니다.
    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> DynamicAltarMaterial;

    // 허공 장막 메시의 런타임 다이내믹 머티리얼입니다.
    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> DynamicHologramMaterial;

    // 구역 내에 서 있는 플레이어 캐릭터들의 동적 배열입니다.
    UPROPERTY()
    TArray<TObjectPtr<class AActor>> OverlappingPlayers;

    // 동기화 수치 기반으로 바닥과 허공 메시의 비주얼 매개변수를 업데이트합니다.
    void UpdateAltarVisuals();

    // 점령 연산 논리를 실시간으로 판단하여 변수를 갱신합니다.
    void ProcessCaptureRules(float DeltaTime);

    // 오버랩된 폰에서 PlayerState를 검출해 팀을 파악하는 유틸리티 함수입니다.
    EFTTeam GetCharacterTeam(class AActor* Actor, bool& bIsValid) const;

    // 클라이언트 쪽에서 리플리케이션 데이터를 전달받았을 때 수신 처리하는 알림 함수입니다.
    UFUNCTION()
    void OnRep_CaptureData();

    // 네트워크 동기화를 지원하는 실제 거점 점령 상태 정보 변수입니다.
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    EFTAltarState CurrentState;

    // 네트워크 동기화를 지원하는 현재 제단 최종 소유 진영 정보 변수입니다.
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    EFTAltarTeam AltarTeam;

    // 현재 게이지 주도권을 형성하고 올려나가는 타겟 팀 정보 변수입니다.
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    EFTAltarTeam CurrentControllingTeam;

    // 실시간 거점의 점령 누적 백분율 혹은 잠금 진행도 수치 변수입니다.
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    float CaptureProgress;

    // 잠금 상태 진입 시 차감되는 실시간 남은 대기 시간 변수입니다.
    UPROPERTY(ReplicatedUsing = OnRep_CaptureData)
    float RemainingLockTime;
};