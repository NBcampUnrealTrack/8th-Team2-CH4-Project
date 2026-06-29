// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FTChatTypes.generated.h"

/**
 * 채팅 공용 로그 카테고리.
 * Output Log에서 "LogFTChat" 으로 필터링하면 전송/분배/수신 흐름을 한 줄기로 추적할 수 있다.
 * 세션(LogFTSession)과 분리해 채팅만 독립적으로 본다.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogFTChat, Log, All);

/**
 * 채팅 채널 구분. 기반 단계에서는 All/System만 실제로 쓰이고,
 * Team은 팀 시스템 도입 시 서버 분배 단계에서 필터링하도록 확장 지점만 열어둔다.
 */
UENUM(BlueprintType)
enum class EFTChatChannel : uint8
{
	All     UMETA(DisplayName = "All (전체)"),
	Team    UMETA(DisplayName = "Team (팀)"),
	System  UMETA(DisplayName = "System (시스템)")
};

/**
 * 한 건의 채팅 메시지. 서버에서 채워져 각 클라이언트로 복제(RPC 인자)되고,
 * 클라이언트 서브시스템이 기록에 보관하며 UI가 표시한다.
 */
USTRUCT(BlueprintType)
struct FFTChatMessage
{
	GENERATED_BODY()

	/** 발신자 표시 이름. System 메시지는 비어 있을 수 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Chat")
	FString SenderName;

	/** 메시지 본문. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Chat")
	FString Message;

	/** 메시지가 속한 채널. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Chat")
	EFTChatChannel Channel = EFTChatChannel::All;

	/** 서버 월드 기준 발신 시각(초). 정렬/표시용. */
	UPROPERTY(BlueprintReadOnly, Category = "FieryTale|Chat")
	double Timestamp = 0.0;
};
