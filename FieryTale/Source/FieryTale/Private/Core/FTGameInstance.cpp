// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/FTGameInstance.h"
#include "Online/FTSessionSubsystem.h"
#include "FieryTaleLog.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UFTGameInstance::Init()
{
	Super::Init();

	//	데디케이티드 서버에서만 서버 부팅 오케스트레이션에 진입한다.
	//	클라이언트/PIE(리슨 서버 포함)는 기존 흐름 그대로 — 여기서 아무것도 하지 않는다.
	if (IsRunningDedicatedServer())
	{
		HandleDedicatedServerStartup();
	}
}

void UFTGameInstance::Shutdown()
{
	//	첫 맵 로드 전에 종료되는 경우를 대비해 일회성 훅을 정리한다.
	if (DediPostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(DediPostLoadMapHandle);
		DediPostLoadMapHandle.Reset();
	}

	Super::Shutdown();
}

void UFTGameInstance::HandleDedicatedServerStartup()
{
	UE_LOG(LogFTSession, Log, TEXT("[DediServer] UFTGameInstance::Init — 데디케이티드 서버 부팅 진입 (첫 월드 로드 후 세션 생성 예약)"));

	//	GameInstance::Init 시점엔 월드/넷드라이버/세션 인터페이스가 아직 준비되지 않았다.
	//	첫 맵(ServerDefaultMap = 대기방) 로드가 끝난 뒤에 데디 세션을 생성하도록 일회성으로 예약한다.
	DediPostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UFTGameInstance::OnPostLoadMap_CreateDedicatedSession);
}

void UFTGameInstance::OnPostLoadMap_CreateDedicatedSession(UWorld* LoadedWorld)
{
	//	최초 1회만 세션을 생성한다. (대기방 로드 시 생성 → 이후 아레나 SeamlessTravel에선 세션이 유지되므로 재생성하지 않는다.)
	if (bDedicatedSessionRequested)
	{
		return;
	}
	bDedicatedSessionRequested = true;

	//	일회성 훅 해제.
	if (DediPostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(DediPostLoadMapHandle);
		DediPostLoadMapHandle.Reset();
	}

	UFTSessionSubsystem* Session = GetSubsystem<UFTSessionSubsystem>();
	if (!Session)
	{
		UE_LOG(LogFTSession, Error, TEXT("[DediServer] UFTSessionSubsystem 없음 — 데디 세션 생성 불가"));
		return;
	}

	const int32 MaxPlayers = GetDedicatedMaxPlayers();
	const FString RoomName = GetDedicatedRoomName();
	UE_LOG(LogFTSession, Log, TEXT("[DediServer] 첫 월드(%s) 로드 완료 → 데디 세션 생성 요청 (MaxPlayers=%d, RoomName=%s)"),
		LoadedWorld ? *LoadedWorld->GetMapName() : TEXT("null"), MaxPlayers, *RoomName);

	Session->CreateDedicatedSession(MaxPlayers, RoomName);
}

int32 UFTGameInstance::GetDedicatedMaxPlayers() const
{
	//	기본 2명(테스트용). 실행 시 -FTMaxPlayers=N 으로 덮어쓴다.
	int32 MaxPlayers = 2;
	FParse::Value(FCommandLine::Get(), TEXT("FTMaxPlayers="), MaxPlayers);
	return FMath::Clamp(MaxPlayers, 1, 32);
}

FString UFTGameInstance::GetDedicatedRoomName() const
{
	FString RoomName = TEXT("FieryTale Dedicated");
	FParse::Value(FCommandLine::Get(), TEXT("FTRoomName="), RoomName);
	return RoomName;
}
