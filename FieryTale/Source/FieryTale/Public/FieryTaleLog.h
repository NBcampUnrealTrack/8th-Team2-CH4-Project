// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * FieryTale 네트워크 흐름(세션 생성/검색/입장 + 로비 접속/Ready 복제/매치 트래블)을
 * 단계별로 추적하기 위한 공용 로그 카테고리.
 *
 * Output Log 또는 콘솔에서 "LogFTSession" 으로 필터링하면 방 만들기/찾기/입장부터
 * 대기방 접속·준비 동기화·매치 트래블까지 한 줄기로 볼 수 있다.
 * 간헐적으로만 재현되는 네트워크 문제를 사후에 로그만으로 진단하기 위한 용도.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogFTSession, Log, All);
