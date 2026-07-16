#include "Level/FTBush.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/FTTags.h"

AFTBush::AFTBush()
{
	PrimaryActorTick.bCanEverTick = false; // 틱 기능 비활성화
	bReplicates = true; // 네트워크 복제 활성화
	
	BushMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BushMesh")); // 비주얼 메쉬 생성
	BushMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메쉬 충돌 비활성화
	BushMesh->SetCollisionProfileName(TEXT("NoCollision")); // 노콜리전 프로필 강제 설정
	RootComponent = BushMesh; // 메쉬를 루트로 설정
	
	OverlapVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapVolume")); // 감지 영역 생성
	OverlapVolume->SetupAttachment(RootComponent); // 루트에 감지 영역 부착
	OverlapVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // 판정 전용 연산 활성화
	OverlapVolume->SetCollisionResponseToAllChannels(ECR_Ignore); // 모든 채널 무시 설정
	OverlapVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 폰 채널 오버랩 설정
	
	OpacityParameterName = TEXT("InvisibilityAlpha"); // 머티리얼 파라미터 이름 설정
}

void AFTBush::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); // 상위 복제 프로퍼티 호출
	DOREPLIFETIME(AFTBush, Occupants); // 진입 배열 복제 등록
	DOREPLIFETIME(AFTBush, RevealedOccupants); // 발각 배열 복제 등록
}

void AFTBush::BeginPlay()
{
	Super::BeginPlay(); // 기저 시작 함수 호출
	if (HasAuthority())
	{
		OverlapVolume->OnComponentBeginOverlap.AddDynamic(this, &AFTBush::OnOverlapBegin); // 진입 오버랩 바인딩
		OverlapVolume->OnComponentEndOverlap.AddDynamic(this, &AFTBush::OnOverlapEnd); // 이탈 오버랩 바인딩
	}
	if (BushMesh && BushMesh->GetMaterial(0))
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(BushMesh->GetMaterial(0), this); // 동적 머티리얼 생성
		BushMesh->SetMaterial(0, DynamicMaterial); // 머티리얼 할당
	}
}

void AFTBush::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return; // 서버 권한 확인
	if (APawn* OverlappedPawn = Cast<APawn>(OtherActor))
	{
		Occupants.AddUnique(OverlappedPawn); // 진입 목록 추가
		SetInvisibilityTag(OverlappedPawn, true); // 은신 상태 태그 부여
		UpdateBushEffects(); // 연출 최신화
	}
}

void AFTBush::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return; // 서버 권한 확인
	if (APawn* OverlappedPawn = Cast<APawn>(OtherActor))
	{
		if (Occupants.Contains(OverlappedPawn))
		{
			Occupants.Remove(OverlappedPawn); // 진입 목록에서 소거
			RevealedOccupants.Remove(OverlappedPawn); // 발각 목록에서 소거
			if (FTimerHandle* ExistingTimer = RevealTimers.Find(OverlappedPawn))
			{
				GetWorld()->GetTimerManager().ClearTimer(*ExistingTimer); // 타이머 초기화
				RevealTimers.Remove(OverlappedPawn); // 타이머 맵 정리
			}
			SetInvisibilityTag(OverlappedPawn, false); // 은신 태그 회수
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OverlappedPawn))
			{
				if (!ASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invisibility))
				{
					SetPawnVisibilityLocal(OverlappedPawn, true); // 외형 노출 원복
				}
			}
			else
			{
				SetPawnVisibilityLocal(OverlappedPawn, true); // 외형 노출 원복
			}
			UpdateBushEffects(); // 연출 최신화
		}
	}
}

void AFTBush::RevealOccupant(APawn* Occupant)
{
	if (!HasAuthority() || !Occupant) return; // 유효성 검사
	if (Occupants.Contains(Occupant))
	{
		RevealedOccupants.AddUnique(Occupant); // 발각 장부 등록
		SetInvisibilityTag(Occupant, false); // 은신 태그 회수
		UpdateBushEffects(); // 연출 최신화
		FTimerManager& TimerManager = GetWorld()->GetTimerManager(); // 타이머 관리자 참조
		if (FTimerHandle* ExistingTimer = RevealTimers.Find(Occupant))
		{
			TimerManager.ClearTimer(*ExistingTimer); // 기존 시간 초기화
		}
		FTimerHandle NewTimer; // 신규 핸들 생성
		FTimerDelegate TimerDel; // 델리게이트 생성
		TimerDel.BindUObject(this, &AFTBush::RestoreInvisibility, Occupant); // 복구 바인딩
		TimerManager.SetTimer(NewTimer, TimerDel, 1.0f, false); // 타이머 가동
		RevealTimers.Add(Occupant, NewTimer); // 장부 기록
	}
}

void AFTBush::RestoreInvisibility(APawn* Occupant)
{
	if (!HasAuthority() || !Occupant) return; // 유효성 검사
	if (Occupants.Contains(Occupant))
	{
		RevealedOccupants.Remove(Occupant); // 발각 장부 해제
		SetInvisibilityTag(Occupant, true); // 은신 태그 재부여
		UpdateBushEffects(); // 연출 최신화
	}
	RevealTimers.Remove(Occupant); // 타이머 제거
}

void AFTBush::SetInvisibilityTag(APawn* Occupant, bool bAddTag)
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Occupant))
	{
		if (bAddTag) ASC->AddLooseGameplayTag(FTTags::FTStates::Buff::Invisibility, 1, EGameplayTagReplicationState::TagAndCountToAll); // 은신 태그 주입
		else ASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::Invisibility, 1, EGameplayTagReplicationState::TagAndCountToAll); // 은신 태그 회수
	}
}

void AFTBush::SetPawnVisibilityLocal(APawn* Pawn, bool bVisible)
{
	if (Pawn && Pawn->GetRootComponent())
	{
		Pawn->GetRootComponent()->SetVisibility(bVisible, true); // 가시성 조절
	}
}

void AFTBush::OnRep_Occupants(const TArray<APawn*>& OldOccupants)
{
	for (APawn* OldPawn : OldOccupants)
	{
		if (OldPawn && !Occupants.Contains(OldPawn))
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OldPawn))
			{
				if (!ASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invisibility))
				{
					SetPawnVisibilityLocal(OldPawn, true); // 외형 노출 복원
				}
			}
			else
			{
				SetPawnVisibilityLocal(OldPawn, true); // 외형 노출 복원
			}
		}
	}
	UpdateBushEffects(); // 연출 동기화 호출
}

void AFTBush::OnRep_RevealedOccupants()
{
	UpdateBushEffects(); // 연출 동기화 호출
}

void AFTBush::UpdateBushEffects()
{
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0); // 로컬 제어기 식별
	if (!LocalPC) return; // 예외 차단
	APawn* LocalPlayer = LocalPC->GetPawn(); // 로컬 플레이어 복사
	if (!LocalPlayer) return; // 예외 차단
	
	UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(LocalPlayer); // 로컬 어빌리티 추출
	if (!LocalASC) return; // 예외 차단
	
	bool bIsLocalBlue = LocalASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue); // 블루팀 판독
	bool bIsLocalRed = LocalASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red); // 레드팀 판독
	bool bLocalTeamHasVision = false; // 가시 플래그 초기화
	
	if (Occupants.Contains(LocalPlayer))
	{
		bLocalTeamHasVision = true; // 본인 상주 시 시각 공유
	}
	else
	{
		for (APawn* Occupant : Occupants)
		{
			if (UAbilitySystemComponent* OccupantASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Occupant))
			{
				if ((bIsLocalBlue && OccupantASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) ||
					(bIsLocalRed && OccupantASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)))
				{
					bLocalTeamHasVision = true; // 아군 상주 시 시각 공유
					break;
				}
			}
		}
	}
	
	bool bIsLocalInBush = Occupants.Contains(LocalPlayer); // 본인 상주 체크
	if (DynamicMaterial)
	{
		float TargetAlpha = bIsLocalInBush ? 0.3f : 1.0f; // 알파 지정
		DynamicMaterial->SetScalarParameterValue(OpacityParameterName, TargetAlpha); // 파라미터 갱신
	}
	
	for (APawn* Occupant : Occupants)
	{
		if (!Occupant) continue; // 예외 거름
		if (Occupant == LocalPlayer)
		{
			SetPawnVisibilityLocal(Occupant, true); // 자신 표시
			continue;
		}
		UAbilitySystemComponent* OccupantASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Occupant); // 어빌리티 확인
		bool bIsSameTeam = false; // 아군 판단 초기화
		if (OccupantASC)
		{
			if ((bIsLocalBlue && OccupantASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Blue)) ||
				(bIsLocalRed && OccupantASC->HasMatchingGameplayTag(FTTags::FTFaction::Team_Red)))
			{
				bIsSameTeam = true; // 진영 일치 확인
			}
		}
		bool bShouldBeVisible = false; // 플래그 초기화
		if (bIsSameTeam)
		{
			bShouldBeVisible = true; // 아군 표시
		}
		else
		{
			if (bLocalTeamHasVision || RevealedOccupants.Contains(Occupant))
			{
				bShouldBeVisible = true; // 시각 공유 또는 발각 시 표시
			}
		}
		SetPawnVisibilityLocal(Occupant, bShouldBeVisible); // 가시성 최종 적용
	}
}