// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Player/NomalAttack/DataAsset/FT_WeaponData.h"
#include "Animation/AnimMontage.h"

UFT_WeaponData::UFT_WeaponData()
	: FireType(EWeaponFireType::LineTrace)
	, BaseDamage(30.f)
	, AttackRange(3000.f)
	, bRootOwnerDuringAttack(false)
	, MovementSpeedMultiplier(1.0f)
	, bAllowHeadshot(false)
	, HeadshotMultiplier(2.0f)
	, InitBaseSpread(0.0f)     
	, InitMaxSpread(0.0f)
	, ProjectileClass(nullptr)
	, ProjectilesPerShot(1)
	, BurstDelay(0.0f)
	, MaxBounceCount(0)
	, AttackMontage(nullptr)
{
	// 데이터 에셋 생성 시 기본 전투 능력치 및 AOS 진영 스펙에 맞추어 초기화 리스트를 구동합니다
}