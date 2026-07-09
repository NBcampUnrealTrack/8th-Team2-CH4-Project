// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTNexus.generated.h"

class UStaticMeshComponent;
class UGeometryCollectionComponent;
class UDataTable;
class USoundBase;
class UParticleSystem;

UENUM(BlueprintType)
enum class EFTNexusTeam : uint8
{
	NoneTeam, // 매크로 충돌 방지를 위한 안전 기본값
	BlueTeam, // 블루팀 식별자
	RedTeam // 레드팀 식별자
};

UCLASS()
class FIERYTALE_API AFTNexus : public AFTTowerBase
{
	GENERATED_BODY()

public:
	AFTNexus(); // 넥서스 생성자

	EFTNexusTeam GetNexusTeam() const { return NexusTeam; } // 본진 소속 진영 반환 함수

protected:
	virtual void BeginPlay() override; // 넥서스 특화 데이터베이스 연동 루틴

	virtual FGameplayTag GetTeamTag() const override; // 진영 태그 오버라이드
	virtual FGameplayTag GetStructureTag() const override; // 타입 태그 오버라이드
	virtual float GetDefaultMaxHealth() const override { return 5000.f; } // 본진 전용 5000 체력 반환 오버라이드

	virtual void PerformDestructionEffects() override; // 1P/2P 메쉬 스왑 및 카오스 재생용 렌더링 오버라이드
	virtual void NotifyGameMode() override; // 1P 게임 오버 통보용 오버라이드

	UFUNCTION(BlueprintImplementableEvent, Category = "Chaos")
	void OnNexusDestroyed(); // 블루프린트 카오스 캐시 재생을 위해 엔진으로 넘겨주는 가상 이벤트

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> NexusMesh; // 블루프린트 오작동을 막기 위해 보존된 원본 넥서스 메쉬

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> DebrisMesh; // 붕괴 연출을 위한 카오스 파편 컬렉션

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UDataTable> NexusDataTable; // 데이터 시트 참조 슬롯

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName; // 데이터 시트 열 식별자

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nexus Property")
	EFTNexusTeam NexusTeam; // 편집 가능한 진영 변수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> DestructionSound; // 파괴 시 출력할 폭발음

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> DestructionEffect; // 파괴 시 출력할 흙먼지 이펙트
};