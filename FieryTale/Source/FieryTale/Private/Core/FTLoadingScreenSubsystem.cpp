// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/FTLoadingScreenSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"
#include "FieryTaleLog.h"

void UFTLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	GetGameInstance()->OnNotifyPreClientTravel().AddUObject(this, &UFTLoadingScreenSubsystem::HandleNotifyPreClientTravel);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UFTLoadingScreenSubsystem::HandlePostLoadMapWithWorld);
}

void UFTLoadingScreenSubsystem::ConfigureLoadingScreen(TSubclassOf<UUserWidget> InLoadingWidgetClass, const TArray<TSoftObjectPtr<UTexture2D>>& InBackgroundImages)
{
	LoadingWidgetClass = InLoadingWidgetClass;
	BackgroundImages = InBackgroundImages;
}

void UFTLoadingScreenSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->OnNotifyPreClientTravel().RemoveAll(this);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Deinitialize();
}

void UFTLoadingScreenSubsystem::HandleNotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	FString MapPart = PendingURL;
	int32 QuestionMarkIndex = INDEX_NONE;
	if (MapPart.FindChar(TEXT('?'), QuestionMarkIndex))
	{
		MapPart.LeftInline(QuestionMarkIndex);
	}
	const FString ShortName = FPackageName::GetShortName(MapPart);
	
	// 목적지가 아레나("Main")일 때만 로딩 화면을 띄운다.
	if (ShortName != TEXT("Main"))
	{
		return;
	}

	bLoadingScreenActive = true;
	bLocalCharacterReady = false;
	bAllPlayersReady = false;
	ShowLoadingScreen();
}

void UFTLoadingScreenSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!bLoadingScreenActive)
	{
		return;
	}

	// 월드가 바뀌면서 이전 뷰포트에 붙어있던 위젯은 이미 사라졌으므로, 새 월드에 다시 붙인다.
	LoadingWidgetInstance = nullptr;
	ShowLoadingScreen();
}

void UFTLoadingScreenSubsystem::ShowLoadingScreen()
{

	if (LoadingWidgetClass && !LoadingWidgetInstance)
	{
		LoadingWidgetInstance = CreateWidget<UUserWidget>(GetGameInstance(), LoadingWidgetClass);
		if (LoadingWidgetInstance)
		{
			if (UImage* BackgroundImage = Cast<UImage>(LoadingWidgetInstance->GetWidgetFromName(TEXT("BackgroundImage"))))
			{
				if (BackgroundImages.Num() > 0)
				{
					UTexture2D* ChosenTexture = BackgroundImages[FMath::RandHelper(BackgroundImages.Num())].LoadSynchronous();
					if (ChosenTexture)
					{
						BackgroundImage->SetBrushFromTexture(ChosenTexture, false);
					}
				}
			}

			LoadingWidgetInstance->AddToViewport(10000);
		}
	}
}

void UFTLoadingScreenSubsystem::NotifyLocalCharacterReady()
{
	bLocalCharacterReady = true;
	TryHideLoadingScreen();
}

void UFTLoadingScreenSubsystem::NotifyAllPlayersReady()
{
	bAllPlayersReady = true;
	TryHideLoadingScreen();
}

void UFTLoadingScreenSubsystem::TryHideLoadingScreen()
{
	if (!bLoadingScreenActive || !bLocalCharacterReady || !bAllPlayersReady)
	{
		return;
	}

	bLoadingScreenActive = false;

	if (LoadingWidgetInstance)
	{
		LoadingWidgetInstance->RemoveFromParent();
		LoadingWidgetInstance = nullptr;
	}
}
