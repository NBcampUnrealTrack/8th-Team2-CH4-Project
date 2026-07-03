#include "UI/Health/FTHealthWidgetComponent.h"
#include "UI/Health/FTHealthWidget.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"

UFTHealthWidgetComponent::UFTHealthWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // 컴포넌트 실시간 틱 업데이트 활성화
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false; // 데디케이트 서버 내 연산 제외
	PrimaryComponentTick.TickInterval = 0.016f; // 프레임 저하 방지용 업데이트 주기 할당

	SetWidgetSpace(EWidgetSpace::World); // 위젯 공간을 3D 월드로 고정
	SetDepthPriorityGroup(SDPG_World); // 렌더링 우선순위를 일반 월드로 설정
	SetGeometryMode(EWidgetGeometryMode::Plane); // 위젯 형태를 평면 스타일로 지정
	SetCastShadow(false); // 가상 위젯의 월드 그림자 생성 기능 비활성화
}

void UFTHealthWidgetComponent::InitWidget()
{
	Super::InitWidget(); // 부모 컴포넌트 초기화 명세 호출
	UpdateASCBinding(); // 데이터 결속용 ASC 추적 및 바인딩
}

void UFTHealthWidgetComponent::BeginPlay()
{
	Super::BeginPlay(); // 부모 게임 시작 수명주기 호출
	AutoPositionWidget(); // 최초 스폰 시 메시 내부에 파묻히지 않도록 머리 위 기초 고도 선제 확보
	UpdateASCBinding(); // 런타임 진입 시점 최종 데이터 결속
}

void UFTHealthWidgetComponent::OnRegister()
{
	Super::OnRegister(); // 월드 서브시스템 컴포넌트 등록 실행
	UpdateASCBinding(); // 에디터 로드 단계 조기 데이터 바인딩 실행
}

void UFTHealthWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	AActor* OwnerActor = GetOwner(); // 소유주 액터 포인터 스캔
	if (OwnerActor)
	{
		if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			FVector CameraLoc = CameraManager->GetCameraLocation(); // 현재 활성화된 카메라 월드 좌표 스캔
			FRotator CameraRot = CameraManager->GetCameraRotation(); // 현재 활성화된 카메라 회전축 수신
			SetWorldRotation(FRotator(0.0f, CameraRot.Yaw + 180.0f, 0.0f)); // 항상 카메라 정면을 응시하도록 회전값 강제 고정

			FVector Origin = OwnerActor->GetActorLocation(); // 루트 실좌표 복사
			FVector BoxExtent(0.0f, 0.0f, 100.0f); // 데이터 부재 대비 디폴트 외곽 크기 설정

			if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
			{
				FBoxSphereBounds RootBounds = RootComp->Bounds; // 위젯 컴포넌트 본인을 연산에서 완전 배제하기 위해 루트 컴포넌트의 순수 영역만 추출
				Origin = RootBounds.Origin;
				BoxExtent = RootBounds.BoxExtent;
			}

			FVector BaseOverheadLoc = FVector(OwnerActor->GetActorLocation().X, OwnerActor->GetActorLocation().Y, Origin.Z + BoxExtent.Z + AdditionalZOffset); // 머리 위 기본 배치 기준 좌표 연산
			FVector DirToCamera = (CameraLoc - BaseOverheadLoc).GetSafeNormal(); // 기점에서 카메라 방향을 가리키는 단위 벡터 산출
			FVector FinalWorldLoc = BaseOverheadLoc + (DirToCamera * PullDistance); // 블루프린트 가변 수치를 반영하여 카메라 방향 좌표 견인

			SetWorldLocation(FinalWorldLoc); // 가공 처리된 최종 위치를 월드 컴포넌트에 할당
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction); // 모든 트랜스폼 연산이 확정된 후 부모 틱을 호출하여 정상 렌더링 가동
}

void UFTHealthWidgetComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (UFTHealthWidget* HealthWidget = Cast<UFTHealthWidget>(GetUserWidgetObject()))
	{
		HealthWidget->InitializeWithAbilitySystem(InASC); // 슬레이트 데이터 소스 포인터 최종 주입
	}
}

void UFTHealthWidgetComponent::UpdateASCBinding()
{
	if (GetWorld() == nullptr || !GetWorld()->IsGameWorld()) return; // 런타임 가상 월드가 아닐 경우 차단

	if (AActor* OwnerActor = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			InitializeWithAbilitySystem(ASC); // 소유 액터 소유 GAS 능력 컴포넌트 최종 결속
		}
	}
}

void UFTHealthWidgetComponent::AutoPositionWidget()
{
	if (AActor* OwnerActor = GetOwner())
	{
		FVector Origin = OwnerActor->GetActorLocation(); // 루트 좌표 초기화
		FVector BoxExtent(0.0f, 0.0f, 100.0f); // 디폴트 높이 마진 세팅

		if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
		{
			FBoxSphereBounds RootBounds = RootComp->Bounds; // 루트 컴포넌트 순수 바운드 추출
			Origin = RootBounds.Origin;
			BoxExtent = RootBounds.BoxExtent;
		}

		const float LocalTopZ = (Origin.Z + BoxExtent.Z) - OwnerActor->GetActorLocation().Z; // 정점 최고 높이 추출
		SetRelativeLocation(FVector(0.0f, 0.0f, LocalTopZ + AdditionalZOffset)); // 틱 연산 가동 전 안전한 초기 위치 고정
	}
}