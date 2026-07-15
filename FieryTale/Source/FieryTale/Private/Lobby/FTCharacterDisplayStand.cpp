// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTCharacterDisplayStand.h"
#include "Character/FTCharacterTypes.h"
#include "Engine/World.h"
#include "Components/SceneCaptureComponent2D.h"

// Sets default values
AFTCharacterDisplayStand::AFTCharacterDisplayStand()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(RootComponent);
	
	SceneCaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComp"));
	SceneCaptureComp->SetupAttachment(RootComponent);
	
	// 'Show Only List'에 등록된 액터만 렌더링하도록 모드를 강제합니다.
	SceneCaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
}

void AFTCharacterDisplayStand::UpdateCharacter(EFTCharacterType NewType)
{
	if (SpawnedCharacter)
	{
		if (SceneCaptureComp)
		{
			SceneCaptureComp->ShowOnlyActors.Remove(SpawnedCharacter);
		}
		SpawnedCharacter->Destroy();
		SpawnedCharacter = nullptr;
	}
	
	if (NewType == EFTCharacterType::None || !CharacterClasses.Contains(NewType))
	{
		return;
	}
	
	TSubclassOf<AActor> ClassToSpawn = CharacterClasses[NewType];
	if (ClassToSpawn)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner(); // 진열대의 오너(플레이어 컨트롤러 등)를 액터 오너로 지정
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FTransform SpawnTransform = SpawnPoint->GetComponentTransform();
		SpawnedCharacter = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnTransform, SpawnParams);
        
		if (SpawnedCharacter)
		{
			SpawnedCharacter->SetReplicates(false); // 복제 비활성화 명시
			SpawnedCharacter->AttachToComponent(SpawnPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			// 🌟 새로 스폰된 캐릭터를 캡처 카메라의 Show Only 배열에 찔러넣습니다!
			if (SceneCaptureComp)
			{
				SceneCaptureComp->ShowOnlyActors.Add(SpawnedCharacter);
			}
		}
	}
}

// Called when the game starts or when spawned
void AFTCharacterDisplayStand::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFTCharacterDisplayStand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

