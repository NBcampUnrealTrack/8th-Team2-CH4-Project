#include "Level/FTBush.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

AFTBush::AFTBush()
{
    PrimaryActorTick.bCanEverTick = false; // 틱 활성화 비활성화

    BushMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BushMesh")); // 스태틱 메시 컴포넌트 생성
    RootComponent = BushMesh; // 루트 컴포넌트로 지정

    OverlapVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapVolume")); // 콜리전 컴포넌트 생성
    OverlapVolume->SetupAttachment(RootComponent); // 메시 컴포넌트에 부착
    OverlapVolume->SetCollisionProfileName(TEXT("Trigger")); // 트리거 콜리전 프로파일 설정

    OpacityParameterName = TEXT("InvisibilityAlpha"); // 투명도 파라미터 이름 기본값 지정
}

void AFTBush::BeginPlay()
{
    Super::BeginPlay(); // 부모 BeginPlay 호출

    OverlapVolume->OnComponentBeginOverlap.AddDynamic(this, &AFTBush::OnOverlapBegin); // 진입 이벤트 바인딩
    OverlapVolume->OnComponentEndOverlap.AddDynamic(this, &AFTBush::OnOverlapEnd); // 이탈 이벤트 바인딩

    if (BushMesh && BushMesh->GetMaterial(0)) // 메시에 할당된 기본 머티리얼이 있는지 유효성 검사
    {
        DynamicMaterial = UMaterialInstanceDynamic::Create(BushMesh->GetMaterial(0), this); // 기존 머티리얼을 기반으로 다이내믹 인스턴스 생성
        BushMesh->SetMaterial(0, DynamicMaterial); // 생성된 다이내믹 인스턴스를 메시에 덮어씌움
    }
}

void AFTBush::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    APawn* OverlappedPawn = Cast<APawn>(OtherActor); // 1인칭 관전 카메라(DefaultPawn) 감지를 위해 APawn으로 캐스팅
    if (OverlappedPawn) // 폰 유효성 검증
    {
        Occupants.AddUnique(OverlappedPawn); // 부쉬 내부 인원 배열에 추가
        UpdateBushEffects(); // 부쉬 상태 업데이트 호출
    }
}

void AFTBush::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    APawn* OverlappedPawn = Cast<APawn>(OtherActor); // APawn으로 캐스팅
    if (OverlappedPawn && Occupants.Contains(OverlappedPawn)) // 대상이 배열에 존재하는지 확인
    {
        Occupants.Remove(OverlappedPawn); // 배열에서 해당 폰 제거
        OverlappedPawn->SetActorHiddenInGame(false); // 가시성 렌더링 복구
        UpdateBushEffects(); // 상태 업데이트 재호출
    }
}

void AFTBush::UpdateBushEffects()
{
    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0); // 로컬 플레이어 컨트롤러 캐싱
    if (!LocalPC) return; // 컨트롤러 유효성 검사

    APawn* LocalPlayer = LocalPC->GetPawn(); // 조종 중인 비행 폰 가져오기
    if (!LocalPlayer) return; // 폰 유효성 검사

    bool bIsLocalInBush = Occupants.Contains(LocalPlayer); // 비행 카메라의 부쉬 존재 여부 판별

    if (DynamicMaterial) // 다이내믹 머티리얼 유효성 확인
    {
        float TargetAlpha = bIsLocalInBush ? 0.3f : 1.0f; // 목표 투명도 수치 산출
        DynamicMaterial->SetScalarParameterValue(OpacityParameterName, TargetAlpha); // 머티리얼 투명도 파라미터 적용
    }

    for (TObjectPtr<APawn> Occupant : Occupants) // 배열 내 모든 폰 순회
    {
        if (Occupant == LocalPlayer) continue; // 본인은 가시성 조작에서 제외

        /* // ====================================================================================
        // [추후 피아식별 로직 구현 공간]
        // ====================================================================================
        */

        if (bIsLocalInBush) // 비행 카메라가 부쉬 안에 진입해 있는 경우
        {
            Occupant->SetActorHiddenInGame(false); // 내부 액터 렌더링 켜기
        }
        else // 비행 카메라가 부쉬 밖에 있는 경우
        {
            Occupant->SetActorHiddenInGame(true); // 내부 액터 렌더링 숨기기
        }
    }
}