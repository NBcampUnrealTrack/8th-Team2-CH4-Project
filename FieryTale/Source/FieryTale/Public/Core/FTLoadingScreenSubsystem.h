// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FTLoadingScreenSubsystem.generated.h"

class UUserWidget;

// 아레나 진입 로딩 화면을 리플리케이션과 무관하게(엔진 PreLoadMap 델리게이트 기준) 표시한다.
// GameInstanceSubsystem이라 GameInstanceClass 설정을 건드리지 않고 자동 등록된다.
UCLASS(config = Game)
class FIERYTALE_API UFTLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// config로 저장 — 네이티브 클래스라 BP 서브클래스 없이 이 CDO 값을 DefaultGame.ini에서 직접 관리한다.
	UPROPERTY(config, EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoadingWidgetClass;

protected:
	// 엔진이 맵 로딩을 시작하는 즉시(리플리케이션/액터 스폰보다 먼저, 순수 로컬) 호출된다.
	// 목적지 맵이 아레나("L_Arena")일 때만 로딩 화면을 띄운다.
	void HandlePreLoadMap(const FString& MapName);

	// 새 월드가 다 올라온 직후 호출된다. 뷰포트 위젯은 월드가 바뀌면 함께 사라지므로,
	// 아직 로딩 중 상태(bLoadingScreenActive)면 새 월드의 뷰포트에 다시 추가한다.
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void ShowLoadingScreen();

private:
	UPROPERTY()
	TObjectPtr<UUserWidget> LoadingWidgetInstance;

	// 아레나로 향하는 로딩 구간인지 여부. PreLoadMap에서 true, (지금은 미구현인) Hide 신호에서 false.
	bool bLoadingScreenActive = false;
};
