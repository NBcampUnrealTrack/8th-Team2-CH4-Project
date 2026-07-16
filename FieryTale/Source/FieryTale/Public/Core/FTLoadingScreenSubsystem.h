// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FTLoadingScreenSubsystem.generated.h"

class UUserWidget;
class UTexture2D;

UCLASS()
class FIERYTALE_API UFTLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoadingWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TArray<TSoftObjectPtr<UTexture2D>> BackgroundImages;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ConfigureLoadingScreen(TSubclassOf<UUserWidget> InLoadingWidgetClass, const TArray<TSoftObjectPtr<UTexture2D>>& InBackgroundImages);

	// 로컬 캐릭터 스폰이 끝난 시점(AFTPlayerController::OnPossess / OnRep_PlayerState)에서 호출한다.
	void NotifyLocalCharacterReady();

	// 전 플레이어 스폰 완료 시점(AFTArenaGameState의 매치 상태가 InProgress로 전환)에서 호출한다.
	void NotifyAllPlayersReady();

	// 로비가 ServerTravel 직전(AFTLobbyGameMode::TravelToMatch)에 실제 참여 인원수를 저장해둔다.
	// GameInstanceSubsystem이라 세션(OSS) 유무와 무관하게 셈리스 트래블 전 구간에서 값이 유지된다.
	void SetExpectedPlayerCount(int32 InCount) { ExpectedPlayerCount = InCount; }
	int32 GetExpectedPlayerCount() const { return ExpectedPlayerCount; }

protected:
	// 심리스 트래블은 UEngine::LoadMap()을 거치지 않아 PreLoadMap이 안 걸리므로,
	// 트래블 직전 클라이언트(및 리슨서버 로컬)에서 항상 호출되는 이 델리게이트를 대신 사용한다.
	void HandleNotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);

	// 새 월드가 다 올라온 직후 호출된다. 뷰포트 위젯은 월드가 바뀌면 함께 사라지므로,
	// 아직 로딩 중 상태(bLoadingScreenActive)면 새 월드의 뷰포트에 다시 추가한다.
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void ShowLoadingScreen();

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> LoadingWidgetInstance;

	// 아레나로 향하는 로딩 구간인지 여부. NotifyPreClientTravel에서 true, TryHideLoadingScreen에서 false.
	bool bLoadingScreenActive = false;

	// 로비가 트래블 직전에 저장해둔 실제 참여 인원수 (0이면 미설정).
	int32 ExpectedPlayerCount = 0;

	// 로컬 캐릭터 스폰 완료 여부.
	bool bLocalCharacterReady = false;

	// 전 플레이어 스폰 완료(매치 InProgress 전환) 여부.
	bool bAllPlayersReady = false;

	// 두 조건이 모두 충족됐을 때만 실제로 로딩 화면을 내린다.
	void TryHideLoadingScreen();
};
