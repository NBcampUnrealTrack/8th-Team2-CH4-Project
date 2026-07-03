// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FTLogSubsystem.generated.h"

/**
 * FieryTale 게임 플로우 공용 로그 카테고리.
 * Output Log에서 "LogFieryTale" 로 필터링하면 전 구간 흐름을 한 줄기로 추적할 수 있다.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogFieryTale, Log, All);

/** 로그 심각도. UE_LOG의 Verbosity에 1:1로 매핑된다. */
UENUM(BlueprintType)
enum class EFTLogLevel : uint8
{
	Verbose UMETA(DisplayName = "Verbose"),
	Log     UMETA(DisplayName = "Log (Info)"),
	Warning UMETA(DisplayName = "Warning"),
	Error   UMETA(DisplayName = "Error")
};

class UFTChatSubsystem;

/**
 * 게임 전역 로깅 서브시스템 (GameInstance 부착 → 메인메뉴/로비/매치 트래블에도 유지).
 *
 * 기본 동작은 UE_LOG와 동일하게 Output Log에 남기고,
 * 현재 컨텍스트에 채팅창(UFTChatWidget)이 떠 있으면 같은 내용을
 * System 채널 메시지로 채팅창에도 출력한다.
 *
 * - C++ : FT_LOG(this, EFTLogLevel::Log, TEXT("남은 인원 %d"), Count);
 * - BP  : GetGameInstanceSubsystem(FTLogSubsystem) → Log / LogInfo / LogWarning / LogError
 *
 * 로그는 로컬 인스턴스 기준이다(UE_LOG와 동일). 서버에서 남긴 로그는 서버(호스트)의
 * 채팅창에만 보이며, 모든 클라이언트 채팅에 뿌리려면 별도 채팅 복제 경로를 써야 한다.
 */
UCLASS()
class FIERYTALE_API UFTLogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** WorldContext가 속한 GameInstance의 로그 서브시스템을 가져온다. (PIE 다중 인스턴스 안전) */
	static UFTLogSubsystem* Get(const UObject* WorldContextObject);

	/** 핵심 진입점. UE_LOG로 출력하고, bAlsoToChat이면 채팅창 존재 시 System 메시지로도 표시. */
	UFUNCTION(BlueprintCallable, Category = "FieryTale|Log", meta = (AutoCreateRefTerm = "Message"))
	void Log(EFTLogLevel Level, const FString& Message, bool bAlsoToChat = true);

	UFUNCTION(BlueprintCallable, Category = "FieryTale|Log", meta = (AutoCreateRefTerm = "Message"))
	void LogInfo(const FString& Message, bool bAlsoToChat = true);

	UFUNCTION(BlueprintCallable, Category = "FieryTale|Log", meta = (AutoCreateRefTerm = "Message"))
	void LogWarning(const FString& Message, bool bAlsoToChat = true);

	UFUNCTION(BlueprintCallable, Category = "FieryTale|Log", meta = (AutoCreateRefTerm = "Message"))
	void LogError(const FString& Message, bool bAlsoToChat = true);

	/** 현재 채팅창(리스너)이 떠 있는지. 이 값이 true일 때만 로그가 채팅창에도 표시된다. */
	UFUNCTION(BlueprintPure, Category = "FieryTale|Log")
	bool IsChatAvailable() const;

private:
	/** 채팅 System 메시지 앞에 붙일 심각도 접두사(경고/오류만). */
	static const TCHAR* LevelPrefix(EFTLogLevel Level);

	/** 이 서브시스템이 속한 GameInstance의 채팅 서브시스템을 반환(없으면 nullptr). */
	UFTChatSubsystem* GetChatSubsystem() const;
};

// C++ 호출 편의 매크로: printf 포맷 + 채팅 라우팅.
// 예) FT_LOG(this, EFTLogLevel::Warning, TEXT("남은 인원 %d"), Count);
// WorldContextObject 로 GameInstance를 찾지 못하면 순정 UE_LOG로 폴백한다.
#define FT_LOG(WorldContextObject, Level, Format, ...) \
	do { \
		if (UFTLogSubsystem* _FTLogSys = UFTLogSubsystem::Get(WorldContextObject)) \
		{ _FTLogSys->Log((Level), FString::Printf((Format), ##__VA_ARGS__)); } \
		else { UE_LOG(LogFieryTale, Log, (Format), ##__VA_ARGS__); } \
	} while (0)
