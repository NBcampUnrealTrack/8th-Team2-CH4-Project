// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/FTLoadingScreenSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"
#include "FieryTaleLog.h"

void UFTLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UFTLoadingScreenSubsystem::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UFTLoadingScreenSubsystem::HandlePostLoadMapWithWorld);
}

void UFTLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Deinitialize();
}

void UFTLoadingScreenSubsystem::HandlePreLoadMap(const FString& MapName)
{
	const FString ShortName = FPackageName::GetShortName(MapName);

	// 목적지가 실제 매치 맵("Main")일 때만 로딩 화면을 띄운다. (L_Transition 경유 구간은 우선순위 밖 — 나중에 필요하면 필터에 추가)
	// 주의: FTLobbyGameMode::GameLevel 코드 기본값은 /Game/Maps/L_Arena 지만, 현재 BP_FTLobbyGameMode에서
	// 실제 트래블 대상은 /Game/Level/Main 으로 오버라이드되어 있다. 두 값이 갈라지면 이 필터도 다시 어긋난다.
	if (ShortName != TEXT("Main"))
	{
		UE_LOG(LogFTSession, Log, TEXT("[LoadingScreen] PreLoadMap(%s) - ShortName=%s, 매치 맵 아님 -> 스킵"), *MapName, *ShortName);
		return;
	}

	bLoadingScreenActive = true;
	ShowLoadingScreen();

	UE_LOG(LogFTSession, Log, TEXT("[LoadingScreen] PreLoadMap(%s) - ShortName=%s, 매치 맵 맞음, 로딩 화면 표시"), *MapName, *ShortName);
}

void UFTLoadingScreenSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!bLoadingScreenActive)
	{
		return;
	}

	// 월드가 바뀌면서 이전 뷰포트에 붙어있던 위젯은 이미 사라졌으므로, 새 월드에 다시 붙인다.
	LoadingWidgetInstance = nullptr;
	ShowLoadingScreen();

	UE_LOG(LogFTSession, Log, TEXT("[LoadingScreen] PostLoadMapWithWorld - 새 월드에 로딩 화면 재부착"));
}

void UFTLoadingScreenSubsystem::ShowLoadingScreen()
{
	if (LoadingWidgetClass && !LoadingWidgetInstance)
	{
		LoadingWidgetInstance = CreateWidget<UUserWidget>(GetGameInstance(), LoadingWidgetClass);
		if (LoadingWidgetInstance)
		{
			LoadingWidgetInstance->AddToViewport(10000);
		}
	}
}
