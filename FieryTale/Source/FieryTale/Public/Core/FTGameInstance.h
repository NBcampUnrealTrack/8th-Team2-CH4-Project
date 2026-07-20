// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FTGameInstance.generated.h"

/**
 * FieryTale 게임 인스턴스.
 *
 * 클라이언트/PIE(리슨 서버 포함)에서는 기본 UGameInstance 동작과 동일하다.
 * 데디케이티드 서버에서만 서버 부팅 오케스트레이션 진입점(HandleDedicatedServerStartup)을 실행한다.
 *
 * GameInstance는 로비 → 아레나 SeamlessTravel에도 유지되므로, 서버가 부팅 시 확보한
 * EOS 세션을 매치 내내 붙잡아 두는 자리로 적합하다.
 *
 * Phase 1: 진입점 + 로그만(EOS 없이 데디 서버가 부팅되는지 검증).
 * Phase 2: 여기서 EOS 트러스티드 서버 로그인 → 데디 세션 생성 시퀀스를 시작한다.
 */
UCLASS()
class FIERYTALE_API UFTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

protected:
	/** 데디케이티드 서버 부팅 시퀀스의 단일 진입점. IsRunningDedicatedServer()일 때만 호출된다.
	 *  Init 시점엔 월드/넷드라이버가 아직 없으므로, 첫 맵(대기방) 로드 완료 후 세션을 생성하도록 예약한다. */
	void HandleDedicatedServerStartup();

private:
	/** 첫 월드(대기방) 로드 완료 시 1회 호출 → 데디 세션 생성. 이후 아레나 트래블에선 재생성하지 않는다. */
	void OnPostLoadMap_CreateDedicatedSession(UWorld* LoadedWorld);

	/** 커맨드라인(-FTMaxPlayers= / -FTRoomName=)에서 데디 세션 정원·이름을 읽는다(없으면 기본값). */
	int32 GetDedicatedMaxPlayers() const;
	FString GetDedicatedRoomName() const;

	//	PostLoadMapWithWorld 일회성 훅 핸들.
	FDelegateHandle DediPostLoadMapHandle;

	//	데디 세션 생성을 이미 요청했는지(중복 방지).
	bool bDedicatedSessionRequested = false;
};
