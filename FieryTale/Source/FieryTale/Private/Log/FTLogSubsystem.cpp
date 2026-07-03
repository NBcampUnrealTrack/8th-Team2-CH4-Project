// Fill out your copyright notice in the Description page of Project Settings.


#include "Log/FTLogSubsystem.h"

#include "Chat/FTChatSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogFieryTale);

UFTLogSubsystem* UFTLogSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	// 호출한 객체가 속한 월드 → 그 월드의 GameInstance에서 서브시스템을 가져온다.
	// (PIE에서 클라이언트/서버가 각자 다른 GameInstance를 갖는 상황에서도 올바른 인스턴스를 찾는다.)
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UFTLogSubsystem>();
		}
	}

	return nullptr;
}

void UFTLogSubsystem::Log(EFTLogLevel Level, const FString& Message, bool bAlsoToChat)
{
	// 1) 항상 Output Log에 남긴다 (UE_LOG와 동일한 기본 동작).
	switch (Level)
	{
	case EFTLogLevel::Error:   UE_LOG(LogFieryTale, Error,   TEXT("%s"), *Message); break;
	case EFTLogLevel::Warning: UE_LOG(LogFieryTale, Warning, TEXT("%s"), *Message); break;
	case EFTLogLevel::Verbose: UE_LOG(LogFieryTale, Verbose, TEXT("%s"), *Message); break;
	case EFTLogLevel::Log:
	default:                   UE_LOG(LogFieryTale, Log,     TEXT("%s"), *Message); break;
	}

	// 2) 채팅창이 떠 있으면 같은 내용을 System 메시지로도 표시한다.
	if (!bAlsoToChat)
	{
		return;
	}

	if (UFTChatSubsystem* Chat = GetChatSubsystem())
	{
		// 실제로 채팅 위젯이 바인딩되어 있을 때만 표시(존재하지 않으면 UE_LOG만 남는다).
		if (Chat->HasActiveListener())
		{
			Chat->PushSystemMessage(FString::Printf(TEXT("%s%s"), LevelPrefix(Level), *Message));
		}
	}
}

void UFTLogSubsystem::LogInfo(const FString& Message, bool bAlsoToChat)
{
	Log(EFTLogLevel::Log, Message, bAlsoToChat);
}

void UFTLogSubsystem::LogWarning(const FString& Message, bool bAlsoToChat)
{
	Log(EFTLogLevel::Warning, Message, bAlsoToChat);
}

void UFTLogSubsystem::LogError(const FString& Message, bool bAlsoToChat)
{
	Log(EFTLogLevel::Error, Message, bAlsoToChat);
}

bool UFTLogSubsystem::IsChatAvailable() const
{
	const UFTChatSubsystem* Chat = GetChatSubsystem();
	return Chat && Chat->HasActiveListener();
}

const TCHAR* UFTLogSubsystem::LevelPrefix(EFTLogLevel Level)
{
	switch (Level)
	{
	case EFTLogLevel::Error:   return TEXT("[오류] ");
	case EFTLogLevel::Warning: return TEXT("[경고] ");
	default:                   return TEXT("");
	}
}

UFTChatSubsystem* UFTLogSubsystem::GetChatSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UFTChatSubsystem>();
	}
	return nullptr;
}
