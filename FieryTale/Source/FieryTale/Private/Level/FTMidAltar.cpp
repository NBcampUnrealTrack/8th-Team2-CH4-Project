#include "Level/FTMidAltar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "AbilitySystem/FT_AbilitySystemComponent.h"
#include "AbilitySystem/FT_AttributeSet.h"
#include "GameplayTags/FTTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Character/FTPlayerState.h"
#include "GameplayEffect.h"

AFTMidAltar::AFTMidAltar()
{
	PrimaryActorTick.bCanEverTick = true; // 실시간 점령 상황 판독 연산을 위해 엔진 등록 틱 가동 활성화
	bReplicates = true; // 네트워크 오브젝트 상태 동기화 활성화
	
	AltarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AltarMesh")); // 제단 기본 외형 컴포넌트 빌드
	RootComponent = AltarMesh; // 제단 메쉬를 액터 루트로 등록
	
	HologramRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HologramRingMesh")); // 상태 연출용 원형 링 컴포넌트 빌드
	HologramRingMesh->SetupAttachment(RootComponent); // 루트 컴포넌트 하위에 링 결합 부착
	HologramRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 링 자체 물리 콜리전 전면 제거
	HologramRingMesh->SetCollisionProfileName(TEXT("NoCollision")); // 블루프린트 프리셋 덮어쓰기 오작동을 차단하기 위한 노콜리전 강제 설정
	
	CaptureVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureVolume")); // 점령 트리거 전용 감지 구역 컴포넌트 빌드
	CaptureVolume->SetupAttachment(RootComponent); // 제단 하위에 결합 부착
	CaptureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 트리거용 판정 연산만 활성화
	CaptureVolume->SetCollisionResponseToAllChannels(ECR_Ignore); // 투사체를 포함한 모든 채널 반응을 강제 무시 처리
	CaptureVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 오직 플레이어(Pawn) 객체만 오버랩으로 감지하도록 명시적 오버라이드
	
	AbilitySystemComponent = CreateDefaultSubobject<UFT_AbilitySystemComponent>(TEXT("AbilitySystemComponent")); // 자체 GAS 컴포넌트 빌드
	AbilitySystemComponent->SetIsReplicated(true); // GAS 네트워크 동기 복제 세팅
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal); // 경량 상태이상 모드로 동기 스레드 최적화 인가
	
	AttributeSet = CreateDefaultSubobject<UFT_AttributeSet>(TEXT("AttributeSet")); // 스탯 저장용 세트 컴포넌트 빌드
	
	AltarTeam = EFTAltarTeam::None; // 귀속 완료 진영 무소속 초기값 세팅
	CurrentControllingTeam = EFTAltarTeam::None; // 게이지 소유 시도 진영 무소속 초기값 세팅
	CurrentState = EFTAltarState::Neutral; // 제단 라이프사이클 중립 상태 초기화 세팅
	CaptureProgress = 0.0f; // 누적 점령 진행비 초기 제로화
	RemainingLockTime = 0.0f; // 락 타임 잔여 수치 제로화
	
	TeamColorParameterName = TEXT("TeamColor"); // 버텍스 셰이더 소속 컬러 인자 매핑
	ProgressParameterName = TEXT("Progress"); // 버텍스 셰이더 진행도 바 플로트 인자 매핑
	ContestedParameterName = TEXT("IsContested"); // 버텍스 셰이더 분쟁 상태 제어 인자 매핑
	
	NeutralColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f); // 미점령시 기본 다크 그레이 색상 할당
	BlueTeamColor = FLinearColor(0.0f, 0.2f, 1.0f, 1.0f); // 블루팀 점령 진행시 아쿠아 블루 색상 할당
	RedTeamColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f); // 레드팀 점령 진행시 스칼렛 레드 색상 할당
	
	MaxCaptureTime = 8.0f; // 만점 점령 도달에 도달하는 필수 시간(8초) 설정
	CaptureDecayMultiplier = 1.5f; // 점령권 방어 및 소거 시 감산 적용 가중 배율 세팅
	LockDuration = 60.0f; // 점령 완료 후 난입 금지 고정 시간(60초) 설정
	CaptureBuffClass = nullptr; // 보상용 글로벌 버프 GE 저장 슬롯 초기화
}

void AFTMidAltar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // 기저 프레임 틱 스트림 기동
	if (HasAuthority())
	{
		ProcessCaptureRules(DeltaTime); // 오직 1P 서버 환경 권한에서만 실시간 진영 점령 상태 연산 실행
	}
	UpdateAltarVisuals(); // 멀티 세션 전원에게 동적 머티리얼 파라미터 그래픽 동기화 갱신 지시
}

UAbilitySystemComponent* AFTMidAltar::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent; // GAS 시스템 컴포넌트 주소 참조 반환
}

void AFTMidAltar::SetGenericTeamId(const FGenericTeamId& InTeamID)
{
	TeamId = InTeamID; // 엔진 AI 퍼셉션용 범용 팀 식별 아이디 저장
}

FGenericTeamId AFTMidAltar::GetGenericTeamId() const
{
	return TeamId; // 엔진 AI 퍼셉션용 팀 식별 아이디 반환
}

void AFTMidAltar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); // 부모 프로퍼티 셋 리스트 인가
	DOREPLIFETIME(AFTMidAltar, CurrentState); // 제단 제어 상태 동기화 변수 등록
	DOREPLIFETIME(AFTMidAltar, AltarTeam); // 제단 소속 점령 진영 동기화 변수 등록
	DOREPLIFETIME(AFTMidAltar, CurrentControllingTeam); // 제단 연산 시도 진영 동기화 변수 등록
	DOREPLIFETIME(AFTMidAltar, CaptureProgress); // 점령 게이지 플로트 수치 동기화 변수 등록
	DOREPLIFETIME(AFTMidAltar, RemainingLockTime); // 잠금 지속 타이머 동기화 변수 등록
}

void AFTMidAltar::OpenAltar()
{
	if (!HasAuthority()) return; // 권한 탈출 가드
	AltarTeam = EFTAltarTeam::None; // 귀속 상태 완전 백지화
	CurrentControllingTeam = EFTAltarTeam::None; // 제어권 장부 완전 백지화
	CurrentState = EFTAltarState::Neutral; // 중립 상태로 강제 전환 인가
	CaptureProgress = 0.0f; // 점령 게이지 완전 초기 영률화
	RemainingLockTime = 0.0f; // 잠금 타이머 영률화
	UpdateAltarVisuals(); // 리셋 상태 레이어 그래픽 렌더 최신화
}

void AFTMidAltar::BeginPlay()
{
	Super::BeginPlay(); // 기저 시작 로직 구동
	if (HasAuthority())
	{
		CaptureVolume->OnComponentBeginOverlap.AddDynamic(this, &AFTMidAltar::OnOverlapBegin); // 감지 트리거 오버랩 진입 처리 연동
		CaptureVolume->OnComponentEndOverlap.AddDynamic(this, &AFTMidAltar::OnOverlapEnd); // 감지 트리거 오버랩 이탈 처리 연동
	}
	if (AltarMesh)
	{
		DynamicAltarMaterial = AltarMesh->CreateDynamicMaterialInstance(0); // 제단 바디 머티리얼 인스턴스 할당 추출
	}
	if (HologramRingMesh)
	{
		DynamicHologramMaterial = HologramRingMesh->CreateDynamicMaterialInstance(0); // 원형 이펙트 머티리얼 인스턴스 할당 추출
	}
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this); // 제단 소유 가상 GAS 핸들 정착 완료 지시
		AttributeSet->InitMaxHealth(10000.0f); // 제단 강도 시스템 최대 한계 초기화
		AttributeSet->InitHealth(10000.0f); // 제단 강도 시스템 수치 충전 초기화
	}
	UpdateAltarVisuals(); // 생성 즉시 비주얼 프레임 원복 세팅
}

void AFTMidAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return; // 서버 유효 확인 가드
	bool bValid = false; // 진영 정합 비트 초기값 설정
	GetCharacterTeam(OtherActor, bValid); // 대상 개체 팀 장부 유효 조회
	if (OtherActor && OtherActor != this && bValid)
	{
		OverlappingPlayers.AddUnique(OtherActor); // 유효 대상 점령 감지 구역 명부에 저장
	}
}

void AFTMidAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return; // 서버 유효 확인 가드
	if (OtherActor)
	{
		OverlappingPlayers.Remove(OtherActor); // 구역을 벗어난 액터 목록에서 영구 해제
	}
}

void AFTMidAltar::ProcessCaptureRules(float DeltaTime)
{
	if (CurrentState == EFTAltarState::Locked)
	{
		RemainingLockTime -= DeltaTime; // 락다운 대기 타이머 초 차감 연산
		CaptureProgress = FMath::Clamp(RemainingLockTime / LockDuration, 0.0f, 1.0f); // 남은 시간을 기반으로 시각 표시용 게이지 가치 계산
		if (RemainingLockTime <= 0.0f)
		{
			OpenAltar(); // 고정 점령 지속 시간 만료 시 중립 상태 복귀 개방 처리 함수 기동
		}
		return;
	}
	int32 BlueTeamCount = 0; // 블루팀 인원 변수 초기화
	int32 RedTeamCount = 0; // 레드팀 인원 변수 초기화
	for (int32 i = OverlappingPlayers.Num() - 1; i >= 0; --i)
	{
		AActor* Actor = OverlappingPlayers[i]; // 대상 액터 주소 할당
		if (!IsValid(Actor))
		{
			OverlappingPlayers.RemoveAt(i); // 파괴 잔여 무효 객체 장부 탈락 소거
			continue;
		}
		bool bValid = false; // 무결 체크 비트 초기화
		EFTTeam Team = GetCharacterTeam(Actor, bValid); // 실소속 데이터 수집
		if (bValid)
		{
			if (Team == EFTTeam::Blue) BlueTeamCount++; // 블루 머릿수 가산
			else if (Team == EFTTeam::Red) RedTeamCount++; // 레드 머릿수 가산
		}
	}
	if (BlueTeamCount > 0 && RedTeamCount > 0)
	{
		CurrentState = EFTAltarState::Contested; // 점령 인원이 교차 분쟁 상태이므로 강제 상태 정지 고정 변경 후 예외 탈출
		return;
	}
	bool bHasPlayers = (BlueTeamCount > 0 || RedTeamCount > 0); // 구역 내부 단독 점령 점유자 포착 여부 저장
	if (!bHasPlayers)
	{
		if (CurrentState == EFTAltarState::Contested)
		{
			CurrentState = (AltarTeam == EFTAltarTeam::None) ? EFTAltarState::Neutral : EFTAltarState::Captured; // 난입 이탈 시 기존 완전 귀속 진영 비례 복원 지정
		}
		if (CurrentControllingTeam != EFTAltarTeam::None)
		{
			CurrentState = EFTAltarState::Progressing; // 무인 상태 게이지 역행 상태 전이 인가
			CaptureProgress -= (DeltaTime / MaxCaptureTime) * CaptureDecayMultiplier; // 점령 진행자 전원 이탈에 한정한 기획 사양 자연적 게이지 감산 감소 연산 공식 적용
			if (CaptureProgress <= 0.0f)
			{
				CaptureProgress = 0.0f; // 진행비 영률 하한 고정
				CurrentControllingTeam = EFTAltarTeam::None; // 게이지 지배 권한 무소속화 원복
				CurrentState = EFTAltarState::Neutral; // 제단 중립 원상태 완전 원복 인가
			}
		}
		return;
	}
	EFTAltarTeam ActiveTeam = (BlueTeamCount > 0) ? EFTAltarTeam::BlueTeam : EFTAltarTeam::RedTeam; // 우세 점유 진영 선별 확정
	CurrentState = EFTAltarState::Progressing; // 게이지 작동 모드로 변경
	if (CurrentControllingTeam == EFTAltarTeam::None)
	{
		CurrentControllingTeam = ActiveTeam; // 비어있던 게이지 제어권 획득 지정
		CaptureProgress += DeltaTime / MaxCaptureTime; // 신규 가산 게이지 누적
	}
	else if (CurrentControllingTeam == ActiveTeam)
	{
		CaptureProgress += DeltaTime / MaxCaptureTime; // 연속 점령 게이지 타임 추가 누적
		if (CaptureProgress >= 1.0f)
		{
			CaptureProgress = 1.0f; // 최고 한도치 고정
			AltarTeam = ActiveTeam; // 영구 소속 승인 인가
			CurrentState = EFTAltarState::Locked; // 제단 락다운 상태 잠금 전환
			RemainingLockTime = LockDuration; // 잠금 타이머 리필 기동
			if (CaptureBuffClass)
			{
				EFTTeam WinningTeam = (AltarTeam == EFTAltarTeam::BlueTeam) ? EFTTeam::Blue : EFTTeam::Red; // 버프 수혜 팀 확인 지정
				for (AActor* Actor : OverlappingPlayers)
				{
					bool bValidTeam = false; // 내부 필터 비트 초기화
					EFTTeam Team = GetCharacterTeam(Actor, bValidTeam); // 대상 팀 수집
					if (bValidTeam && Team == WinningTeam)
					{
						APawn* Pawn = Cast<APawn>(Actor); // 캐릭터 포인터 변환
						if (Pawn && Pawn->GetPlayerState())
						{
							AFTPlayerState* FTPS = Cast<AFTPlayerState>(Pawn->GetPlayerState()); // 수혜 플레이어 상태 변환
							if (FTPS && FTPS->GetAbilitySystemComponent())
							{
								UAbilitySystemComponent* ASC = FTPS->GetAbilitySystemComponent(); // GAS 참조 확보
								FGameplayEffectContextHandle Context = ASC->MakeEffectContext(); // 문맥 버퍼 확보
								Context.AddSourceObject(this); // 출처 지정 등록
								FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CaptureBuffClass, 1.0f, Context); // 외보 전송 버프 스펙 생성 빌드
								if (Spec.IsValid())
								{
									ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()); // 수혜 아군 개체 본인에게 버프 부여 완료 실행
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		CaptureProgress -= (DeltaTime / MaxCaptureTime) * CaptureDecayMultiplier; // 타 진영 방해 공작 난입으로 인한 역게이지 차감 연산 공식 가동
		if (CaptureProgress <= 0.0f)
		{
			CaptureProgress = 0.0f; // 하한 원복 고정
			CurrentControllingTeam = ActiveTeam; // 소유권 반전 강탈 변경 등록
			AltarTeam = EFTAltarTeam::None; // 귀속 상태 무소속화 해제
		}
	}
	CaptureProgress = FMath::Clamp(CaptureProgress, 0.0f, 1.0f); // 최종 수치 보정 범위 가둬두기 고정
}

void AFTMidAltar::UpdateAltarVisuals()
{
	FLinearColor MatColor = NeutralColor; // 출력 색상 기본값 바인딩
	if (CurrentState == EFTAltarState::Locked)
	{
		MatColor = NeutralColor; // 완전 락다운 작동시 머티리얼 중립 블랙 계열 컬러 매핑
	}
	else
	{
		if (CurrentControllingTeam == EFTAltarTeam::BlueTeam)
		{
			MatColor = BlueTeamColor; // 게이지 블루 연출 변경
		}
		else if (CurrentControllingTeam == EFTAltarTeam::RedTeam)
		{
			MatColor = RedTeamColor; // 게이지 레드 연출 변경
		}
		if (CurrentState == EFTAltarState::Captured)
		{
			MatColor = (AltarTeam == EFTAltarTeam::BlueTeam) ? BlueTeamColor : RedTeamColor; // 완전 소속 귀속 완료시 해당 진영 고유 컬러 매핑 고정
		}
	}
	float IsContestedValue = (CurrentState == EFTAltarState::Contested) ? 1.0f : 0.0f; // 분쟁 중 점멸 상태 확인 플로트화 연산
	if (DynamicAltarMaterial)
	{
		DynamicAltarMaterial->SetVectorParameterValue(TeamColorParameterName, MatColor); // 셰이더 벡터 색상 인자 반영
		DynamicAltarMaterial->SetScalarParameterValue(ProgressParameterName, CaptureProgress); // 셰이더 플로트 게이지 비 반영
		DynamicAltarMaterial->SetScalarParameterValue(ContestedParameterName, IsContestedValue); // 셰이더 가부 비 반영
	}
	if (DynamicHologramMaterial)
	{
		DynamicHologramMaterial->SetVectorParameterValue(TeamColorParameterName, MatColor); // 셰이더 링 벡터 색상 반영
		DynamicHologramMaterial->SetScalarParameterValue(ProgressParameterName, CaptureProgress); // 셰이더 링 플로트 게이지 비 반영
		DynamicHologramMaterial->SetScalarParameterValue(ContestedParameterName, IsContestedValue); // 셰이더 링 가부 비 반영
	}
}

EFTTeam AFTMidAltar::GetCharacterTeam(AActor* Actor, bool& bIsValid) const
{
	bIsValid = false; // 초기 비트 세팅 기본화
	APawn* Pawn = Cast<APawn>(Actor); // 액터에서 Pawn 추출 캐스팅
	if (!Pawn) return EFTTeam::Blue; // 불일치 이탈 예외 방어
	APlayerState* PS = Pawn->GetPlayerState(); // 소유 제어 상태 장부 참조
	if (!PS) return EFTTeam::Blue; // 불일치 이탈 예외 방어
	AFTPlayerState* FTPS = Cast<AFTPlayerState>(PS); // 커스텀 상태 장부 캐스팅
	if (!FTPS) return EFTTeam::Blue; // 불일치 이탈 예외 방어
	bIsValid = true; // 무결점 통과 락 개방 승인
	return FTPS->GetTeam(); // 확정 소속 인게임 팀 열거형 반환
}

void AFTMidAltar::OnRep_CaptureData()
{
	UpdateAltarVisuals(); // 복제 본문 클라이언트 단 그래픽 보정 즉시 투영 처리 함수 트리거 호출
}