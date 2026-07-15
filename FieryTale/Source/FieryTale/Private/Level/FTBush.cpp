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
    // 틱 비활성화로 성능 최적화
    PrimaryActorTick.bCanEverTick = false;
    // 멀티플레이어 환경 네트워크 복제 활성화
    bReplicates = true;

    // 시각적 메쉬 생성 및 투사체 차단 방지를 위한 콜리전 완전 해제
    BushMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BushMesh"));
    BushMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RootComponent = BushMesh;

    // 트리거 볼륨 생성 및 Pawn 전용 감지 채널 필터링
    OverlapVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapVolume"));
    OverlapVolume->SetupAttachment(RootComponent);
    OverlapVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    OverlapVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    OverlapVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 머티리얼 파라미터 이름 초기화
    OpacityParameterName = TEXT("InvisibilityAlpha");
}

void AFTBush::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 진입자 및 발각자 배열 리플리케이션 등록
    DOREPLIFETIME(AFTBush, Occupants);
    DOREPLIFETIME(AFTBush, RevealedOccupants);
}

void AFTBush::BeginPlay()
{
    Super::BeginPlay();

    // 서버 권한인 경우에만 오버랩 이벤트 바인딩
    if (HasAuthority())
    {
        OverlapVolume->OnComponentBeginOverlap.AddDynamic(this, &AFTBush::OnOverlapBegin);
        OverlapVolume->OnComponentEndOverlap.AddDynamic(this, &AFTBush::OnOverlapEnd);
    }

    // 동적 머티리얼 인스턴스 생성 및 메쉬 적용
    if (BushMesh && BushMesh->GetMaterial(0))
    {
        DynamicMaterial = UMaterialInstanceDynamic::Create(BushMesh->GetMaterial(0), this);
        BushMesh->SetMaterial(0, DynamicMaterial);
    }
}

void AFTBush::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 서버가 아니면 반환
    if (!HasAuthority()) return;

    // 감지된 액터를 폰으로 캐스팅
    if (APawn* OverlappedPawn = Cast<APawn>(OtherActor))
    {
        // 배열에 폰 추가 
        Occupants.AddUnique(OverlappedPawn);
        // GAS AI 감지 불가 태그 부여
        SetInvisibilityTag(OverlappedPawn, true);
        
        // 가시성 및 시야 공유 업데이트
        UpdateBushEffects();
    }
}

void AFTBush::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 서버가 아니면 반환
    if (!HasAuthority()) return;

    // 액터를 폰으로 캐스팅
    if (APawn* OverlappedPawn = Cast<APawn>(OtherActor))
    {
        // 대상이 배열 내에 존재하는지 확인
        if (Occupants.Contains(OverlappedPawn))
        {
            // 배열에서 대상 제거
            Occupants.Remove(OverlappedPawn);
            RevealedOccupants.Remove(OverlappedPawn);

            // 가동 중인 노출 타이머 파기
            if (FTimerHandle* ExistingTimer = RevealTimers.Find(OverlappedPawn))
            {
                GetWorld()->GetTimerManager().ClearTimer(*ExistingTimer);
                RevealTimers.Remove(OverlappedPawn);
            }

            // GAS 태그 카운트 차감
            SetInvisibilityTag(OverlappedPawn, false);

            // 교집합 부쉬가 없어 태그가 완전히 회수되었을 때만 렌더링 복구
            if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OverlappedPawn))
            {
                if (!ASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invisibility))
                {
                    OverlappedPawn->SetActorHiddenInGame(false);
                }
            }
            else
            {
                // 관전 카메라 등 ASC가 없는 액터 즉시 복구
                OverlappedPawn->SetActorHiddenInGame(false);
            }
            
            // 이탈 후 남은 인원들의 시야 재계산
            UpdateBushEffects();
        }
    }
}

void AFTBush::RevealOccupant(APawn* Occupant)
{
    // 서버 권한 및 타겟 유효성 확인
    if (!HasAuthority() || !Occupant) return;

    // 대상이 해당 부쉬 안에 있을 때만 발동
    if (Occupants.Contains(Occupant))
    {
        // 발각자 배열에 등록하여 적에게 시야 개방
        RevealedOccupants.AddUnique(Occupant);
        
        // 적 AI 추적 허용을 위해 은신 태그 일시 회수
        SetInvisibilityTag(Occupant, false);
        
        // 가시성 즉시 반영
        UpdateBushEffects();

        // 연타 방지를 위해 기존 타이머 파기
        FTimerManager& TimerManager = GetWorld()->GetTimerManager();
        if (FTimerHandle* ExistingTimer = RevealTimers.Find(Occupant))
        {
            TimerManager.ClearTimer(*ExistingTimer);
        }

        // 1초 뒤 은신 및 시야 차단을 복원하는 타이머 세팅
        FTimerHandle NewTimer;
        FTimerDelegate TimerDel;
        TimerDel.BindUObject(this, &AFTBush::RestoreInvisibility, Occupant);
        TimerManager.SetTimer(NewTimer, TimerDel, 1.0f, false);
        RevealTimers.Add(Occupant, NewTimer);
    }
}

void AFTBush::RestoreInvisibility(APawn* Occupant)
{
    // 서버 권한 및 타겟 유효성 확인
    if (!HasAuthority() || !Occupant) return;

    // 부쉬 내부에 체류 중이라면 발각 해제 및 AI 은신 태그 복원
    if (Occupants.Contains(Occupant))
    {
        RevealedOccupants.Remove(Occupant);
        SetInvisibilityTag(Occupant, true);
        UpdateBushEffects();
    }
    
    // 핸들 메모리 정리
    RevealTimers.Remove(Occupant);
}

void AFTBush::SetInvisibilityTag(APawn* Occupant, bool bAddTag)
{
    // 대상의 ASC 확보 및 태그 중첩 횟수 조작
    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Occupant))
    {
        if (bAddTag) ASC->AddLooseGameplayTag(FTTags::FTStates::Buff::Invisibility, 1, EGameplayTagReplicationState::TagAndCountToAll);
        else ASC->RemoveLooseGameplayTag(FTTags::FTStates::Buff::Invisibility, 1, EGameplayTagReplicationState::TagAndCountToAll);
    }
}

void AFTBush::OnRep_Occupants(const TArray<APawn*>& OldOccupants)
{
    // 클라이언트 측 배열 이탈 시 교집합 검사 후 가시성 롤백
    for (APawn* OldPawn : OldOccupants)
    {
        if (OldPawn && !Occupants.Contains(OldPawn))
        {
            if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OldPawn))
            {
                if (!ASC->HasMatchingGameplayTag(FTTags::FTStates::Buff::Invisibility))
                {
                    OldPawn->SetActorHiddenInGame(false);
                }
            }
            else
            {
                OldPawn->SetActorHiddenInGame(false);
            }
        }
    }
    
    // 남은 인원 렌더링 갱신
    UpdateBushEffects();
}

void AFTBush::OnRep_RevealedOccupants()
{
    // 타겟 발각 상태 변경 시 즉각적인 화면 갱신
    UpdateBushEffects();
}

void AFTBush::UpdateBushEffects()
{
    // 클라이언트 로컬 플레이어 확보
    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0);
    if (!LocalPC) return;

    APawn* LocalPlayer = LocalPC->GetPawn();
    if (!LocalPlayer) return;
    
    UAbilitySystemComponent* LocalASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(LocalPlayer);
    if (!LocalASC) return;

    // 로컬 플레이어의 팀 판별
    bool bIsLocalBlue = LocalASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Team.Blue")));
    bool bIsLocalRed = LocalASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Team.Red")));

    // 우리 팀이 부쉬에 대한 시야를 가지고 있는지 판별
    bool bLocalTeamHasVision = false;
    if (Occupants.Contains(LocalPlayer))
    {
        bLocalTeamHasVision = true;
    }
    else
    {
        // 내가 없더라도 아군이 부쉬에 있다면 시야 공유
        for (APawn* Occupant : Occupants)
        {
            if (UAbilitySystemComponent* OccupantASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Occupant))
            {
                if ((bIsLocalBlue && OccupantASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Team.Blue")))) ||
                    (bIsLocalRed && OccupantASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Team.Red")))))
                {
                    bLocalTeamHasVision = true;
                    break;
                }
            }
        }
    }

    // 부쉬 자체 메쉬 반투명화
    bool bIsLocalInBush = Occupants.Contains(LocalPlayer);
    if (DynamicMaterial)
    {
        float TargetAlpha = bIsLocalInBush ? 0.3f : 1.0f;
        DynamicMaterial->SetScalarParameterValue(OpacityParameterName, TargetAlpha);
    }

    // 부쉬 내 모든 캐릭터의 렌더링 피아 식별
    for (APawn* Occupant : Occupants)
    {
        if (!Occupant) continue;

        // 로컬 플레이어 본인은 무조건 렌더링을 활성화하여 투명화 버그 원천 차단
        if (Occupant == LocalPlayer)
        {
            Occupant->SetActorHiddenInGame(false);
            continue;
        }

        UAbilitySystemComponent* OccupantASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Occupant);
        bool bIsSameTeam = false;
        
        if (OccupantASC)
        {
            if ((bIsLocalBlue && OccupantASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Team.Blue")))) ||
                (bIsLocalRed && OccupantASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("Team.Red")))))
            {
                bIsSameTeam = true;
            }
        }

        bool bShouldBeVisible = false;

        // 아군은 항상 보이고, 적군은 시야가 있거나 발각되었을 때만 노출
        if (bIsSameTeam)
        {
            bShouldBeVisible = true;
        }
        else
        {
            if (bLocalTeamHasVision || RevealedOccupants.Contains(Occupant))
            {
                bShouldBeVisible = true;
            }
        }

        // 판정에 따른 액터 렌더링 결정
        Occupant->SetActorHiddenInGame(!bShouldBeVisible);
    }
}