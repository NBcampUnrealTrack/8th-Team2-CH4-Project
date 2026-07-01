// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/Character/FT_CharacterData.h"

UFT_CharacterData::UFT_CharacterData()
	: CharacterMesh(nullptr)
	, WeaponData(nullptr)
	, DefaultMaxHealth(500.0f)           // AOS 장르 기준 1레벨 기본 체력 버퍼링 (500)
	, DefaultMaxShield(0.0f)             // 기본 보호막은 0을 표준으로 지정
	, DefaultMoveSpeed(600.0f)           // 언리얼 순정 walking 표준 이동 속도 (6m/s)
	, DefaultAttackPower(1.0f)           // 기본 대미지 계수 100% 가중치 세팅
	, DefaultMaxUltimateGauge(100.0f)    // 궁극기 게이지 마스터 요구량 기본값 (100)
{
	// 데이터 에셋 생성 시 쓰레기 값 오염으로 인한 런타임 크래시를 원천 방쇄하기 위해
	// 멤버 초기화 리스트(Member Initializer List) 파이프라인을 구동합니다.
}