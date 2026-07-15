// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FieryTale : ModuleRules
{
	public FieryTale(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",                  // 기본 자료형/컨테이너/수학 (FString, TArray, FMath 등)
			"CoreUObject",           // UObject·리플렉션 시스템 (UCLASS/USTRUCT/UPROPERTY, 델리게이트)
			"Engine",                // 엔진 런타임 핵심 (Actor/Pawn/World/Component, 게임프레임워크, 복제)
			"InputCore",             // 입력 기본 타입 (EKeys, 키/마우스 식별자)
			"EnhancedInput",         // Enhanced Input 시스템 (InputMappingContext/InputAction) — 캐릭터 조작
			"GameplayAbilities",     // GAS 본체 (AbilitySystemComponent, AttributeSet, GameplayEffect, GA)
			"GameplayTasks",         // GAS 어빌리티 태스크 (PlayMontageAndWait 등) — GameplayAbilities 의존
			"GameplayTags",          // 네이티브 게임플레이 태그 (FTTags 선언/정의)
			"UMG",                   // 위젯 UI (UUserWidget, Button, TextBlock, ScrollBox 등)
			"Slate",                 // UMG 하부 UI 프레임워크 (입력 모드, 위젯 포커스 등)
			"SlateCore",             // Slate 코어 타입/리플렉션 enum (ETextCommit 등) — UFUNCTION 파라미터 링크용
			"OnlineSubsystem",		// 온라인 세션 인터페이스 (IOnlineSession) — 세션 생성/검색/입장
			"OnlineSubsystemEOS",   // EOS 구현체
			"OnlineSubsystemUtils",   // 온라인 보조 유틸 (접속 주소 해석 등)
            "GeometryCollectionEngine",// 카오스 디스트럭션을 위한 모듈
            "ChaosSolverEngine",	  // 카오스 디스트럭션 물리 연산을 위한 모듈
            "ChaosCaching",			 // 카오스 디스트럭션 물리 연산 베이킹을 위한 모듈
            "NavigationSystem"
        });

		// AI 시스템 (AIController/내비게이션 등). 모듈 내부에서만 사용하므로 Private.
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule" });

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
