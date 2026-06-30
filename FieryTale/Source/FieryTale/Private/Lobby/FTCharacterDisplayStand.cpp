// Fill out your copyright notice in the Description page of Project Settings.


#include "Lobby/FTCharacterDisplayStand.h"

// Sets default values
AFTCharacterDisplayStand::AFTCharacterDisplayStand()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(RootComponent);
}

void AFTCharacterDisplayStand::UpdateCharacter(EFTCharacterType NewType)
{
	// 1. 기존에 세워둔 캐릭터가 있다면 지워줍니다.
	if (SpawnedCharacter)
	{
		SpawnedCharacter->Destroy();
		SpawnedCharacter = nullptr;
	}

	// 2. '선택 안 됨'이거나 딕셔너리에 등록되지 않은 타입이면 그냥 비워둡니다.
	if (NewType == EFTCharacterType::None || !CharacterClasses.Contains(NewType))
	{
		return;
	}

	// 3. 에디터에서 매핑해둔 블루프린트 클래스 정보를 가져와 실제 액터로 스폰합니다.
	TSubclassOf<AActor> ClassToSpawn = CharacterClasses[NewType];
	if (ClassToSpawn)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner(); // 🌟 핵심: 진열대의 오너(플레이어 컨트롤러 등)를 액터 오너로 지정
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FTransform SpawnTransform = SpawnPoint->GetComponentTransform();
		SpawnedCharacter = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnTransform, SpawnParams);
        
		// 🌟 복제 활성화 명시
		if (SpawnedCharacter)
		{
			SpawnedCharacter->SetReplicates(true); 
			SpawnedCharacter->AttachToComponent(SpawnPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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

