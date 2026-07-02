// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/FTGameMode.h"
#include "FieryTaleLog.h"
#include "Core/Game/FTArenaGameState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"

#include "Level/FTTurret.h"
#include "Level/FTNexus.h"
#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"

AFTGameMode::AFTGameMode()
{
	bUseSeamlessTravel = true;

	// 매치 정본 PlayerState는 ASC/AttributeSet/팀을 보유한 AFTPlayerState로 통일한다.
	// (구 AFTArenaPlayerState는 ASC가 없어 GAS 전투가 무효였음 — 통합으로 제거)
	PlayerStateClass = AFTPlayerState::StaticClass();
	// 입력/HUD/리스폰이 동작하도록 매치 컨트롤러를 명시적으로 지정한다.
	PlayerControllerClass = AFTPlayerController::StaticClass();
	GameStateClass = AFTArenaGameState::StaticClass();
}

void AFTGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	// 세션 인터페이스로부터 방 생성 시 설정한 정원(MaxPlayers)을 가져옵니다.
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (OSS)
	{
		IOnlineSessionPtr SessionInterface = OSS->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
			if (Session)
			{
				MinPlayersToStart = Session->SessionSettings.NumPublicConnections;
				UE_LOG(LogFTSession, Log, TEXT("[Arena] 세션 연동 완료: 전장 시작 필수 인원 %d명"), MinPlayersToStart);
			}
		}
	}
}

// 1. 일반 신규 접속 시 호출
void AFTGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	UE_LOG(LogFTSession, Log, TEXT("[Arena] HandleStartingNewPlayer 진입 (신규 접속 또는 심리스 초기화): [%s]"), *NewPlayer->GetName());
	
	InitializeAndSpawnPlayer(NewPlayer);
}

// 2. 심리스 트래블로 이동 시 호출
void AFTGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	
	if (APlayerController* NewPlayer = Cast<APlayerController>(C))
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Arena] 심리스 트래블 정착 및 데이터 복사 확인 완료: [%s]"), *NewPlayer->GetName());
		InitializeAndSpawnPlayer(NewPlayer);
	}
}

void AFTGameMode::InitializeAndSpawnPlayer(APlayerController* NewPlayer)
{
	if (NewPlayer == nullptr) return;

	AFTPlayerController* FTPC = Cast<AFTPlayerController>(NewPlayer);
	AFTPlayerState* PS = NewPlayer->GetPlayerState<AFTPlayerState>();
	if (FTPC == nullptr || PS == nullptr)
	{
		// PlayerController/PlayerState 클래스가 BP GameMode에서 AFTPlayerController/AFTPlayerState로
		// 지정되지 않았을 때 여기 걸린다. (에디터 설정 확인 필요)
		UE_LOG(LogFTSession, Error, TEXT("[Arena] 🚨 스폰 실패 - FTPlayerController=%s, FTPlayerState=%s (BP GameMode 클래스 지정 확인)"),
			FTPC ? TEXT("OK") : TEXT("NULL"), PS ? TEXT("OK") : TEXT("NULL"));
		return;
	}

	// 로비에서 선택이 전달되지 않았으면(None) 유효한 기본값으로 보정해 None 스폰 시도를 막는다.
	if (PS->GetSelectedCharacterType() == EFTCharacterType::None)
	{
		PS->SetSelectedCharacterType(EFTCharacterType::Alice);
	}

	// 중복 스폰 방지: 이미 FieryTale 캐릭터를 빙의 중이면 재스폰하지 않는다.
	if (Cast<AFTPlayerCharacterBase>(NewPlayer->GetPawn()) != nullptr)
	{
		UE_LOG(LogFTSession, Log, TEXT("[Arena] [%s] 이미 캐릭터 빙의 중 → 중복 스폰 건너뜀"), *NewPlayer->GetName());
	}
	else
	{
		// 심리스 트래블 후 붙어있던 기본 폰이 있으면 제거한다.
		if (APawn* OldPawn = NewPlayer->GetPawn())
		{
			OldPawn->Destroy();
		}

		// 팀 스폰 지점을 계산한 뒤, GAS 인지 스폰 경로(CharacterData 주입)로 컨트롤러에 위임한다.
		AActor* StartSpot = FindPlayerStart(NewPlayer);
		const FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
		const FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

		UE_LOG(LogFTSession, Log, TEXT("[Arena] [%s] 캐릭터 스폰 위임 (타입=%d)"),
			*NewPlayer->GetName(), (int32)PS->GetSelectedCharacterType());
		FTPC->SpawnCharacter(SpawnLocation, SpawnRotation);
	}

	// 인원수 체크 및 매치 시작 검사
	if (GameState && GameState->PlayerArray.Num() >= MinPlayersToStart)
	{
		StartArenaMatch();
	}
}

void AFTGameMode::StartArenaMatch()
{
	AFTArenaGameState* ArenaGS = GetGameState<AFTArenaGameState>();
	if (ArenaGS)
	{
		// MatchState를 WaitingToStart ➡️ InProgress 로 변경!
		if (ArenaGS->GetArenaMatchState() == EFTArenaMatchState::WaitingToStart)
		{
			ArenaGS->SetArenaMatchState(EFTArenaMatchState::InProgress);
			UE_LOG(LogFTSession, Log, TEXT("[Arena] 모든 플레이어 진입 확인! 전장 전투를 시작합니다."));
			
			// TODO: 여기에 각 플레이어들의 움직임을 허용하거나 카운트다운 UI를 띄우는 신호를 넣으면 됩니다.
		}
	}
}

void AFTGameMode::TurretDestroyed(AFTTurret* DestroyedTurret)	// 포탑 파괴 이벤트 처리 로직 구현
{
	if (!DestroyedTurret)										// 안전검사
	{
		return;
	}

	EFTTurretTeam Team = DestroyedTurret->GetTurretTeam();		// 소스 액터에 구현된 게터를 통해 파괴된 대상 소속 진영 수신
	EFTTurretPosition Position = DestroyedTurret->GetTurretPosition();	// 배치 위치 수신

	FString TeamStr = (Team == EFTTurretTeam::BlueTeam) ? TEXT("Blue Team") : TEXT("Red Team");
	// 진영 열거형 값이 블루팀인 경우와 그 외의 경우를 삼항연산자로 판별해 문자열로 치환 (로직만 구현하고 임시로 문자열 출력)
	FString PosStr = (Position == EFTTurretPosition::Left) ? TEXT("Left") : TEXT("Right");
	// 왼쪽 라인인지 오른쪽 라인인지도 구별

	UE_LOG(LogTemp, Log, TEXT("Notification: %s's %s Turret has been destroyed!"), *TeamStr, *PosStr);
	// 포맷 스트링 형식을 빌려 어떤 팀의 어느 쪽 라인 방어 타워가 파괴되었는지 출력 창에 실시간 기록
}

void AFTGameMode::NexusDestroyed(AFTNexus* DestroyedNexus)
{
	if (!DestroyedNexus)
	{
		return;
	}

	uint8 LosingTeam = static_cast<uint8>(DestroyedNexus->GetNexusTeam());
	uint8 WinningTeam = (LosingTeam == 1) ? 2 : 1;
	HandleGameOver(WinningTeam);
}

void AFTGameMode::HandleGameOver(uint8 WinningTeam)
{
	if (WinningTeam == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("Blue Team Wins"));
	}
	else if (WinningTeam == 2)
	{
		UE_LOG(LogTemp, Log, TEXT("Red Team Wins"));
	}
}