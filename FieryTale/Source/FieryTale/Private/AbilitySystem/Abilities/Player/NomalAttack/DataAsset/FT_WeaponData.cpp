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
	, ProjectilesPerShot(1)
	, BurstDelay(0.0f)
	, MaxBounceCount(0)
	, AttackMontage(nullptr)
{
}