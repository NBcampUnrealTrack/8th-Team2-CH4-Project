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
class UGameplayEffect; // ◄ 전역 GE 전방 선언 추가로 컴파일 속도 최적화 타설

/**
 * FieryTale 전장의 블루 vs 레드 진영 미니언들을 총괄하는 GAS 내장형 C++ 마스터 캐릭터 클래스입니다.
 * 단 1개의 클래스로 데이터 에셋(MinionData)만 교체하여 무한 생산을 집도하는 총괄님 아키텍처 규격을 준수합니다.
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
     *  [심볼 해결 완료 - 런타임 자산 완착 세터]
     * 스포너 단에서 마스터 소체를 Deferred 스폰한 직후, 이번 차례의 무기/외형 데이터 에셋과 
     * 진영 세력 태그를 공차 없이 원자적으로 밀봉 주입해 줄 핵심 퍼블릭 인터페이스입니다.
     */
    FORCEINLINE void SetMinionDataAndTeam(UFT_MinionData* InData, const FGameplayTag& InTeamTag)
    {
        MinionData = InData;
        MinionTeamTag = InTeamTag;
    }

    /** 
     * [심볼 해결 완료 - 인프라 수동 가동 관문]
     * 스포너가 팀 태그, 레일 경로, 미니언 자산 데이터 주입을 완료한 완공 시점에
     * 본체의 내장형 GAS 인프라 및 비주얼 바인딩 시동 틱을 안전하게 켜줄 수동 트리거 함수입니다.
     */
    void LaunchMinionInfrastructure();
    
    TSubclassOf<class AFT_ProjectileBase> GetMinionProjectileClass() const { return MinionProjectileClass; }

protected:
    /** 월드 진입 즉시 실행되던 구형 시동 로직을 보류하고 스포너의 완성 신호를 대기하기 위해 비워둡니다. */
    virtual void BeginPlay() override;

    // =========================================================================
    // [멀티플레이어 네트워크 닻줄 가상 함수 오버라이드 완착]
    // =========================================================================
    /** 서버 단 AI 컨트롤러 포제스 시점에 GAS 하드웨어 연결을 마감하는 포인터 낚시 바늘 */
    virtual void PossessedBy(AController* NewController) override;

    /** 클라이언트 단 네트워크 동기화 복제 시점에 클라이언트 세션 닻줄을 내릴 복제 낚시 바늘 */
    virtual void OnRep_Controller() override;

    /** 미니언 본체에 직통 내장되어 모든 전술 상태 관리 및 어빌리티 격발을 통제할 고유 GAS 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** [리뷰 저격 박멸 - 정석 스탯 장부 보관 슬롯]
     * 생성자에서 공중에 버려지던 AttributeSet 객체를 본체 변수에 안전하게 명시적 바인딩합니다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | GAS")
    TObjectPtr<const class UFT_AttributeSet> AttributeSet;

    /** 기획자가 월드 배치 창(디테일 패널)이나 스폰 매니저에서 낙인찍어줄 미니언의 AOS 팀 세력 태그 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    FGameplayTag MinionTeamTag;

    /** 기획자가 빌드한 미니언 전용 데이터 에셋 (외형 메시, 기본 체력, 이동 속도, 공격 스펙 슬롯) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TObjectPtr<UFT_MinionData> MinionData;

    /** 에디터 디테일 패널에서 C++ 고유 AI 뇌세포인 'UFT_Minion_Brain' 클래스를 정밀 매핑해 주는 주입구입니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TSubclassOf<UGameplayAbility> BrainAbilityClass;

    // =========================================================================
    // 💡 [네트워크 피아식별 안전망 데이터 슬롯 완착]
    // =========================================================================
    /** 블루/레드 팀 진영 태그를 클라이언트 머신까지 무결하게 전파할 전용 무한 지속형 GE 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FieryTale | Minion | Design")
    TSubclassOf<UGameplayEffect> TeamGameplayEffectClass;
    
    /** 미니언이 현재 라인을 타고 정찰 전진하며 실시간으로 쫓아가고 있는 타겟 웨이포인트 포인터입니다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieryTale | AI | Navigation")
    TObjectPtr<AFT_WayPoint> CurrentTargetWayPoint;
    
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Minion | GAS")
    TSubclassOf<class AFT_ProjectileBase> MinionProjectileClass;

private:
    /** 주입된 데이터 에셋 수치 동기화, 피아식별 태그 낙인, AI 브레인 시동을 원자적으로 통합 처리하는 공정 함수 */
    void InitializeMinionInfrastructures();
    
};