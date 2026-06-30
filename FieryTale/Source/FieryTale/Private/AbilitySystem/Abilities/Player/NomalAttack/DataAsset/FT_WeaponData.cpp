// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Animation/AnimMontage.h"

UFT_WeaponData::UFT_WeaponData()
	: FireType(EWeaponFireType::LineTrace) // 기본 무기 타입은 즉시 발사(빨간 망토 스펙)로 세팅
	, BaseDamage(30.f)                      // 기획 스펙: 기본 평타 대미지 30
	, AttackRange(3000.f)                  // 기본 사거리 30m
	, bRootOwnerDuringAttack(false)
	, MovementSpeedMultiplier(1.0f)        // 공격 중 이동 속도 패널티 없음(100% 속도)을 기본값으로 지정
	, bAllowHeadshot(false)
	, HeadshotMultiplier(2.0f)             // 헤드샷 성공 시 기본 2배율 가산
	, InitBaseSpread(0.0f)     
	, InitMaxSpread(0.0f)
	, ProjectileClass(nullptr)
	, ProjectilesPerShot(1)                // 투사체 영웅 변환 시 오작동을 막기 위한 최소 1개 보장
	, BurstDelay(0.05f)                    // 연사 투사체 스폰 시 크래시를 방지하기 위한 최소 안전 프레임 딜레이
	, MaxBounceCount(0)
	, AttackMontage(nullptr)
{
	// 데이터 에셋 생성 시 기본 전투 능력치 및 FieryTale 시스템 규격에 맞추어 초기화 리스트를 구동합니다
}