// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/FTGameMode.h"
#include "FieryTaleLog.h"
#include "Character/FTPlayerState.h"
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
#include "FieryTaleLog.h"
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
	
	// 컨트롤러 캐스팅 (통합된 FTPlayerController 사용)
	AFTPlayerController* FTPlayerPC = Cast<AFTPlayerController>(NewPlayer);
	if (!FTPlayerPC) return;
	
	if (GameState) // 게임 스테이트는 게임모드에서 관리
	{
		/** GameState의 PlayerArray에서 현재 접속한 플레이어의 인덱스를 찾습니다. PlayerController::AssignTeam 호출하면
		PlayerState::AssignTeamTag 을 호출하게 되고 개별 플레어의 팀 태그를 지정할 수 있습니다. **/
		
		int32 PlayerIndex = GameState->PlayerArray.Find(FTPlayerPC->GetPlayerState<APlayerState>());
		
		EFTTeam AssignedTeam = (PlayerIndex % 2 == 0) ? EFTTeam::Blue : EFTTeam::Red;
		
		FTPlayerPC->AssignTeam(AssignedTeam);

		UE_LOG(LogFTSession, Log, TEXT("[Team] 🛡️ 플레이어 [%s]님에게 %s 팀이 배정되었습니다! (Index: %d)"), 
			*NewPlayer->GetName(), (AssignedTeam == EFTTeam::Blue) ? TEXT("Blue") : TEXT("Red"), PlayerIndex);
	}

	AFTPlayerState* Ps = FTPlayerPC->GetPlayerState<AFTPlayerState>();
	if (Ps)
	{
		EFTCharacterType ChosenCharacter = Ps->GetSelectedCharacterType();
       
		if (ChosenCharacter == EFTCharacterType::None)
		{
			ChosenCharacter = EFTCharacterType::Alice;
			Ps->SetSelectedCharacterType(ChosenCharacter); // State에도 기본값 덮어쓰기
		}

		bool bAlreadySpawned = false;

		// 현재 폰이 진짜 '우리 게임의 캐릭터'인지 클래스 타입으로 검사
		if (APawn* CurrentPawn = FTPlayerPC->GetPawn())
		{
			if (CurrentPawn->IsA(AFTPlayerCharacterBase::StaticClass()))
			{
				bAlreadySpawned = true;
				UE_LOG(LogFTSession, Log, TEXT("[ArenaDebug] [%s] 님은 이미 진짜 캐릭터가 세팅되어 있습니다. 중복 스폰을 건너뜁니다."), *NewPlayer->GetName());
			}
		}

		if (!bAlreadySpawned)
		{
			// 기존에 빙의하고 있던 쓸모없는 기본 폰(SpectatorPawn 등) 파괴
			if (APawn* OldPawn = NewPlayer->GetPawn())
			{
				OldPawn->Destroy(); 
			}

			// PlayerStart 위치 찾기
			AActor* StartSpot = FindPlayerStart(NewPlayer);
			FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
			FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

			// 컨트롤러에게 데이터 주입 및 스폰 명령
			FTPlayerPC->SpawnCharacter(SpawnLocation, SpawnRotation);
    
			UE_LOG(LogFTSession, Log, TEXT("[ArenaDebug] 플레이어 [%s] 캐릭터 스폰 요청 완료!"), *NewPlayer->GetName());
		}
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