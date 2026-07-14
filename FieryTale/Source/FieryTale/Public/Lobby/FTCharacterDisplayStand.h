// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/FTPlayerState.h"
#include "Character/FTCharacterTypes.h"
#include "FTCharacterDisplayStand.generated.h"

UCLASS()
class FIERYTALE_API AFTCharacterDisplayStand : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFTCharacterDisplayStand();
	
	// 에디터의 레벨에 배치할 때 직접 지정할 '자리 번호' (0, 1, 2, 3...)
	UPROPERTY(EditInstanceOnly, Category = "Display")
	int32 StandIndex = 0;

	// 핵심: 캐릭터 타입과 임시 블루프린트 액터 클래스를 매핑하는 오브젝트 포인터 딕셔너리
	UPROPERTY(EditDefaultsOnly, Category = "Display")
	TMap<EFTCharacterType, TSubclassOf<AActor>> CharacterClasses;
	
	// 캐릭터가 소환될 위치를 지정할 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Display")
	class USceneComponent* SpawnPoint;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Display")
	USceneCaptureComponent2D* SceneCaptureComp;
	
	// 캐릭터가 변경되었을 때 호출될 함수
	void UpdateCharacter(EFTCharacterType NewType);
	
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY()
	AActor* SpawnedCharacter;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
