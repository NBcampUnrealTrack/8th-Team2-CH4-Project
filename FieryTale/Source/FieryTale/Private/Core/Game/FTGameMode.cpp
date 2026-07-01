// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Game/FTGameMode.h"
#include "FieryTaleLog.h"
#include "Lobby/FTLobbyPlayerState.h"
#include "Core/Game/FTArenaGameState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Core/Player/FTArenaPlayerState.h"
#include "Online/OnlineSessionNames.h"

#include "Level/FTTurret.h"
#include "Level/FTNexus.h"
#include "FieryTaleLog.h"
#include "Character/FTPlayerController.h"
#include "Character/FTPlayerState.h"

AFTGameMode::AFTGameMode()
{
	bUseSeamlessTravel = true;
	
	PlayerStateClass = AFTArenaPlayerState::StaticClass();
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
	
	AFTArenaPlayerState* ArenaPS = NewPlayer->GetPlayerState<AFTArenaPlayerState>();
	if (ArenaPS)
	{
		EFTCharacterType ChosenCharacter = ArenaPS->SelectedCharacter;
       
		// 🌟 주의: None일 때 다시 None을 주면 절대 안 됩니다! 존재하는 캐릭터(예: Warrior)로 할당해 주세요.
		if (ChosenCharacter == EFTCharacterType::None)
		{
			ChosenCharacter = EFTCharacterType::Alice; // 유효한 기본 Enum 값으로 변경 필수!
		}

		// 🌟 [핵심 추가]: 중복 소환 방지 로직
		bool bAlreadySpawned = false;
		UClass* TargetClass = CharacterClasses.Contains(ChosenCharacter) ? CharacterClasses[ChosenCharacter] : nullptr;
        
		// 현재 플레이어가 가진 폰이, 우리가 스폰하려던 바로 그 클래스(TargetClass)라면 이미 성공한 것입니다.
		if (TargetClass && NewPlayer->GetPawn() && NewPlayer->GetPawn()->IsA(TargetClass))
		{
			UE_LOG(LogFTSession, Log, TEXT("[ArenaDebug] [%s] 님은 이미 캐릭터가 세팅되어 있습니다. 중복 스폰을 건너뜁니다."), *NewPlayer->GetName());
			bAlreadySpawned = true;
		}

		// 아직 스폰되지 않았을 때(첫 번째 호출일 때)만 폰을 소환합니다.
		if (!bAlreadySpawned)
		{
			UE_LOG(LogFTSession, Log, TEXT("[ArenaDebug] 복사 완료된 캐릭터 타입 확인: %d"), (int32)ChosenCharacter);

			APawn* SpawnedPawn = SpawnCharacterForPlayer(NewPlayer, ChosenCharacter);
			if (SpawnedPawn)
			{
				if (APawn* OldPawn = NewPlayer->GetPawn())
				{
					OldPawn->Destroy(); // 기존에 빙의하던 기본 구체 폰 파괴
				}
				
				SpawnedPawn->SetOwner(NewPlayer);
				NewPlayer->Possess(SpawnedPawn);
				NewPlayer->ClientSetViewTarget(SpawnedPawn);
                
				UE_LOG(LogFTSession, Log, TEXT("[ArenaDebug] 플레이어 [%s] 캐릭터 스폰 및 빙의 성공!"), *NewPlayer->GetName());
			}
			else
			{
				UE_LOG(LogFTSession, Error, TEXT("[ArenaDebug] 🚨 캐릭터 스폰 실패. 타입 번호: %d"), (int32)ChosenCharacter);
			}
		}
	}
	else
	{
		UE_LOG(LogFTSession, Error, TEXT("[ArenaDebug] 🚨 여전히 AFTArenaPlayerState를 찾지 못했습니다."));
	}

	// 인원수 체크 및 매치 시작 검사
	if (GameState)
	{
		if (GameState->PlayerArray.Num() >= MinPlayersToStart)
		{
			StartArenaMatch();
		}
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

APawn* AFTGameMode::SpawnCharacterForPlayer(APlayerController* PC, EFTCharacterType CharacterType)
{
	if (!CharacterClasses.Contains(CharacterType) || CharacterClasses[CharacterType] == nullptr)
	{
		UE_LOG(LogFTSession, Warning, TEXT("[Arena] CharacterClasses 맵에 BP가 등록되지 않았습니다."));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// 맵에 배치된 PlayerStart 위치 찾기
	AActor* StartSpot = FindPlayerStart(PC);
	FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
	FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PC;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 매핑해둔 블루프린트 클래스로 실제 캐릭터 스폰
	TSubclassOf<APawn> ClassToSpawn = CharacterClasses[CharacterType];
	return World->SpawnActor<APawn>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);
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