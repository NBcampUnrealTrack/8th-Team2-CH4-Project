// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Character/FTCharacterBase.h"
#include "FT_MinionCharacterBase.generated.h"

class UAbilitySystemComponent;
class UFT_MinionData;
class UGameplayAbility;
class AFT_WayPoint;
class UGameplayEffect;

/**
 * FieryTale 전장의 블루 vs 레드 진영 미니언들을 총괄하는 GAS 내장형 C++ 마스터 캐릭터 클래스입니다.
 * 단 1개의 클래스로 데이터 에셋(MinionData)만 교체하여 무한 생산을 집도하는 총괄님 아키측처 규격을 준수합니다.
 */
UCLASS()
class FIERYTALE_API AFT_MinionCharacterBase : public AFTCharacterBase
{
    GENERATED_BODY()

public:
    AFT_MinionCharacterBase();

    /** [IAbilitySystemInterface 순정 오버라이드] 미니언 본체 고유의 내장형 ASC 주소지를 사출합니다. */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

    // =========================================================================
    // [AOS 내비게이션 및 런타임 데이터 에셋 주입 배관망]
    // =========================================================================

    /** 스포너가 런타임에 미니언을 스폰하자마자 초도 거점 경로 주소를 밀봉 이식할 직통 함수 */
    UFUNCTION(BlueprintCallable, Category = "FieryTale | AI | Navigation")
    void SetCurrentTargetWayPoint(AFT_WayPoint* InWayPoint) { CurrentTargetWayPoint = InWayPoint; }

    /** 
     * 브레인 어빌리티(UFT_Minion_Brain) 단에서 현재 미니언이 전진해야 할 
     * 최신 거점 주소를 무결하게 역산 추출해 갈 수 있도록 열어주는 전용 장부 게터입니다.
     */
    FORCEINLINE AFT_WayPoint* GetCurrentTargetWayPoint() const { return CurrentTargetWayPoint; }

    /** 
     * [런타임 자산 완착 세터]
     * 스포너 단에서 마스터 소체를 Deferred 스폰한 직후, 이번 차례의 무기/외형 데이터 에셋과 
     * 진영 세력 태그를 공차 없이 원자적으로 밀봉 주입해 줄 핵심 퍼블릭 인터페이스입니다.
     */
    void SetMinionDataAndTeam(UFT_MinionData* InMinionData, const FGameplayTag& InTeamTag);

    /** 
     * [인프라 수동 가동 관문]
     * 스포너가 팀 태그, 레일 경로, 미니언 자산 데이터 주입을 완료한 완공 시점에
     * 본체의 내장형 GAS 인프라 및 비주얼 바인딩 시동 틱을 안전하게 켜줄 수동 트리거 함수입니다.
     */
    void LaunchMinionInfrastructure();
    
    FORCEINLINE TSubclassOf<class AFT_ProjectileBase> GetMinionProjectileClass() const { return MinionProjectileClass; }

    // =========================================================================
    // [멀티플레이어 네트워크 리플리케이션 명세 장부 선언]
    // =========================================================================
    /** 멀티플레이어 변수 동기화 원자 규격을 선언하기 위한 순정 가상 함수 */
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    virtual void Die(AController* KillerController) override;

protected:
    virtual void BeginPlay() override;

    /** 서버 단 AI 컨트롤러 포제스 시점에 GAS 하드웨어 연결을 마감하는 포인터 낚시 바늘 */
    virtual void PossessedBy(AController* NewController) override;

    // 💡 [넷코드 유령 선로 완벽 소각]:
    // AI가 조종하는 폰의 Controller 변수는 클라이언트로 복제되지 않으므로 영원히 격발되지 않던
    // virtual void OnRep_Controller() override; 명세를 헤더에서 흔적도 없이 깔끔하게 청소했습니다.

    // =========================================================================
    // [RepNotify 콜백 배관]
    // =========================================================================
    /** 클라이언트 프록시 단에 미니언 데이터 자산 복제가 안착하는 순간 비주얼 레이어를 강제 갱신할 기폭 함수 */
    UFUNCTION()
    void OnRep_MinionData();

protected:
    /** 미니언 본체에 직통 내장되어 모든 전술 상태 관리 및 어빌리티 격발을 통제할 고유 GAS 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** 생성자에서 공중에 버려지던 AttributeSet 객체를 본체 변수에 안전하게 명시적 바인딩합니다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | GAS")
    TObjectPtr<const class UFT_AttributeSet> AttributeSet;

    /** 팀 진영 태그가 원격지 클라이언트로 무결하게 복제되도록 마킹합니다. */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    FGameplayTag MinionTeamTag;

    /** 데이터 에셋이 클라이언트로 도달하는 순간 비주얼 동기화가 격발되도록 전용 기폭 장치(OnRep)를 바인딩합니다. */
    UPROPERTY(ReplicatedUsing = OnRep_MinionData, EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TObjectPtr<UFT_MinionData> MinionData;

    /** 에디터 디테일 패널에서 C++ 고유 AI 뇌세포인 'UFT_Minion_Brain' 클래스를 정밀 매핑해 주는 주입구입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TSubclassOf<UGameplayAbility> BrainAbilityClass;

    /** 블루/레드 팀 진영 태그를 클라이언트 머신까지 무결하게 전파할 전용 무한 지속형 GE 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TSubclassOf<UGameplayEffect> TeamGameplayEffectClass;
    
    /** 미니언이 현재 라인을 타고 정찰 전진하며 실시간으로 쫓아가고 있는 타겟 웨이포인트 포인터입니다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | AI | Navigation")
    TObjectPtr<AFT_WayPoint> CurrentTargetWayPoint;
    
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Minion | GAS")
    TSubclassOf<class AFT_ProjectileBase> MinionProjectileClass;
    
    /** 💡 [주무기 컴포넌트] */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minion | Visual")
    TObjectPtr<UStaticMeshComponent> MainWeaponComponent;

    /** 💡 [보조무기 컴포넌트] */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minion | Visual")
    TObjectPtr<UStaticMeshComponent> SecondaryWeaponComponent;

private:
    /** 주입된 데이터 에셋 수치 동기화, 피아식별 태그 낙인, AI 브레인 시동을 원자적으로 통합 처리하는 공정 함수 */
    void InitializeMinionInfrastructures();
    
    /** 💡 [클라이언트 동기화 배관 명세]: Dead 태그 추가 시 서버/클라이언트 양단 각자 로컬 충돌체를 완전 파기해 줄 델리게이트 콜백 */
    void OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
    
    UPROPERTY()
    TSubclassOf<class UAnimInstance> FallbackAnimClass;
};