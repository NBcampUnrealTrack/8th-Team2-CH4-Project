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
#include "Level/FTBattleSubsystem.h"
#include "FieryTaleLog.h"
#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"
#include "Character/FTPlayerCharacterBase.h"
#include "Online/FTSessionSubsystem.h"
#include "GameFramework/GameSession.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Level/FTTeamPlayerStart.h"
#include "EngineUtils.h"
#include "Core/FTLoadingScreenSubsystem.h"

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

	// 세션(OSS)이 없거나 정원 조회에 실패하는 환경(PIE 등)에서도 정확한 값을 쓰도록,
	// 로비가 트래블 직전에 저장해둔 실제 인원수가 있으면 그 값을 우선한다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UFTLoadingScreenSubsystem* LoadingScreen = GI->GetSubsystem<UFTLoadingScreenSubsystem>())
		{
			const int32 LobbyPlayerCount = LoadingScreen->GetExpectedPlayerCount();
		}
	}

	if (AFTArenaGameState* ArenaGS = GetGameState<AFTArenaGameState>())
	{
		ArenaGS->SetExpectedPlayerCount(MinPlayersToStart);
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

	AFTPlayerState* Ps = FTPlayerPC->GetPlayerState<AFTPlayerState>();
	if (Ps)
	{
		int32 MyIndex = Ps->GetPlayerIndex();
		
		// (안전장치) 혹시라도 에디터에서 바로 아레나 맵을 켜서 인덱스가 없는 경우 순차 배정
		if (MyIndex == -1 && GameState) 
		{
			MyIndex = GameState->PlayerArray.Find(Ps);
			if (MyIndex == INDEX_NONE) MyIndex = GameState->PlayerArray.Num();
		}

		// 인덱스를 기준으로 짝수(0, 2, 4...)는 블루, 홀수(1, 3, 5...)는 레드
		EFTTeam AssignedTeam = (MyIndex % 2 == 0) ? EFTTeam::Blue : EFTTeam::Red;
		FTPlayerPC->AssignTeam(AssignedTeam);

		UE_LOG(LogFTSession, Log, TEXT("[Team] 🛡️ 플레이어 [%s]님에게 %s 팀이 배정되었습니다! (고유 Index: %d)"), 
			*NewPlayer->GetName(), (AssignedTeam == EFTTeam::Blue) ? TEXT("Blue") : TEXT("Red"), MyIndex);
		
		
		
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
			if (APawn* OldPawn = NewPlayer->GetPawn())
			{
				OldPawn->Destroy(); 
			}

			// 🌟 [변경]: 배정받은 팀(AssignedTeam)에 맞는 스폰 지점 찾기
			TArray<AFTTeamPlayerStart*> ValidSpawns;
		
			for (TActorIterator<AFTTeamPlayerStart> It(GetWorld()); It; ++It)
			{
				if (It->SpawnTeam == AssignedTeam)
				{
					ValidSpawns.Add(*It);
				}
			}

			AActor* StartSpot = nullptr;
			if (ValidSpawns.Num() > 0)
			{
				// 팀에 맞는 스폰 포인트 중 하나를 무작위로 선택 (스폰 겹침 방지)
				int32 RandomIndex = FMath::RandRange(0, ValidSpawns.Num() - 1);
				StartSpot = ValidSpawns[RandomIndex];
			}
			else
			{
				// 만약 맵에 팀 전용 스폰이 하나도 없다면 기존 방식으로 폴백(Fallback)
				StartSpot = FindPlayerStart(NewPlayer);
				UE_LOG(LogFTSession, Warning, TEXT("[Spawn] %s 팀 전용 스폰 지점이 없습니다! 기본 스폰을 사용합니다."), 
					(AssignedTeam == EFTTeam::Blue) ? TEXT("Blue") : TEXT("Red"));
			}

			FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
			FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

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
	
	if (UFTBattleSubsystem* Battle = GetWorld()->GetSubsystem<UFTBattleSubsystem>())
	{
		Battle->NotifyTurretDestroyed(DestroyedTurret);
	}
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
	AFTArenaGameState* ArenaGS = GetGameState<AFTArenaGameState>();

	// 재진입 방지: 넥서스 이벤트 중복/양측 파괴 등으로 두 번 호출되어도 종료 절차는 1회만 수행한다.
	if (ArenaGS && ArenaGS->GetArenaMatchState() == EFTArenaMatchState::GameOver)
	{
		return;
	}

	UE_LOG(LogFTSession, Log, TEXT("[Arena] 매치 종료! 우승 팀: %s"), (WinningTeam == 1) ? TEXT("Blue") : TEXT("Red"));

	// 서버 주도로 전 세계의 시간을 0.2배속으로 늦춥니다. (자동으로 클라이언트에 동기화됨)
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.2f);
	
	FTimerDelegate ResultUIDelegate = FTimerDelegate::CreateUObject(this, &AFTGameMode::ShowResultUI, WinningTeam);
	GetWorldTimerManager().SetTimer(GameOverSequenceTimerHandle, ResultUIDelegate, 0.6f, false);

	// Gamemode는 서버만 소유하고 잇으므로 UI는 playercontroller에서 처리
}

void AFTGameMode::ShowResultUI(uint8 WinningTeam)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AFTPlayerController* PC = Cast<AFTPlayerController>(It->Get()))
		{
			PC->DisableInput(PC);
		}
	}
	
	//  모든 접속자(클라이언트)에게 결과창 UI를 띄우라고 RPC 명령 송출
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AFTPlayerController* PC = Cast<AFTPlayerController>(It->Get()))
		{
			PC->Client_ShowResultUI(WinningTeam);
		}
	}

	// 2. 결과창을 띄운 상태로 감상할 시간 제공 후 최종 로비 복귀
	// 0.2배속 상태에서 현실 시간 5초 대기 = 1.0f
	GetWorldTimerManager().SetTimer(GameOverSequenceTimerHandle, this, &AFTGameMode::EndSessionAndReturn, 1.0f, false);
}

void AFTGameMode::EndSessionAndReturn()
{
	// 1. 다음 맵 이동 및 정상화를 위해 시간을 1.0으로 복구
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

	// 2. 온라인 세션 파괴 및 메인 레벨로 복귀 (기존 로직 유지)
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFTSessionSubsystem* Session = GameInstance->GetSubsystem<UFTSessionSubsystem>())
		{
			Session->LeaveSession();
		}
	}

	if (GameSession)
	{
		GameSession->ReturnToMainMenuHost();
	}
}