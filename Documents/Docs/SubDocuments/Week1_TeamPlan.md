# FieryTale 4-Week Prototype: Week 1 Team Plan

## Purpose

이 문서는 FieryTale을 5인 프로그래머 팀이 4주 개발 일정으로 착수할 때, 첫 1주일에 어느 정도 구현 산출물이 나와야 하는지와 초기 업무 분배안을 정리한다.

현재 팀 구성 가정은 다음과 같다.

- 프로그래머 5명
- 그중 2명은 기획 역량을 추가 학습 중
- 그중 다른 2명은 TA 역량을 추가 학습 중
- 프로젝트 목표는 Unreal Engine 기반 TPS/AOS 멀티플레이 프로토타입
- 전체 개발기간은 4주

다음 단계에서는 각 팀원의 상세 스펙을 반영해 이 분배안을 다시 조정한다.

## Week 1 Goal

1주차의 목표는 완성도 높은 게임 화면이 아니라, 가장 큰 기술 리스크를 빠르게 검증하는 것이다.

FieryTale의 주요 리스크는 다음이다.

- Unreal Engine 멀티플레이
- Dedicated Server 또는 서버 권한 기반 매치 흐름
- Gameplay Ability System, GAS
- TPS 이동, 카메라, 조준, 공격 감각
- AOS 규칙: 팀, 구조물, 넥서스, 승패
- 임시 에셋과 애니메이션 연결 파이프라인

따라서 1주차 종료 시점에는 "2v2 TPS AOS의 가장 얇은 한 판이 서버 권한으로 돌아가는 상태"를 목표로 한다.

## Expected Week 1 Result

1주차 종료 시점의 최소 구현 목표는 다음이다.

- 2명의 클라이언트가 같은 서버에 접속 가능
- 플레이어 캐릭터가 팀을 나눠 스폰됨
- TPS 이동, 카메라, 조준 방향 동기화
- 서버 권한 기반 기본 공격 1종
- Health Attribute와 피격/사망 처리
- 임시 타워 또는 넥서스 1종 배치
- 구조물이 피해를 받고 파괴됨
- 넥서스 파괴 시 승패 로그 또는 간단 UI 출력
- 2라인 구조의 회색 박스 테스트맵
- 캐릭터/무기/이펙트는 임시 에셋 사용

1주차에는 미니언, 성장 시스템, 캐릭터별 복수 스킬, 최종 UI, 최종 아트는 목표에서 제외한다.

## Target Play Flow

1주차 빌드는 아래 흐름을 끝까지 검증할 수 있어야 한다.

1. 서버 실행
2. 클라이언트 2명에서 4명 접속
3. 팀 A/B로 스폰
4. 캐릭터 이동
5. 상대 또는 구조물을 조준해 기본 공격
6. 체력 감소
7. 사망/리스폰 또는 구조물 파괴
8. 넥서스 파괴 시 승패 판정

## Team Role Simulation

| Member | Main Role | Week 1 Responsibility |
| --- | --- | --- |
| A | Lead / Network / Game Rules | 서버 접속, 팀 배정, 스폰, 매치 시작/종료, 승패 판정 |
| B | GAS / Combat | AttributeSet, 기본 공격 Ability, Damage GameplayEffect, Death 처리 |
| C | TPS Character / Input | 이동, 카메라, 조준, 입력 바인딩, 임시 전투 조작감 |
| D | TA 1 | 캐릭터/무기 임시 에셋 적용, Animation Blueprint, 피격 피드백 |
| E | Planning + TA 2 | 2라인 맵 블록아웃, 구조물 배치, 규칙 문서, 테스트 체크리스트 |

기획 역량을 학습 중인 인원은 문서 작성만 담당하지 않고, 구현 가능한 규칙을 빠르게 확정하고 테스트 결과를 다시 룰에 반영한다.

TA 역량을 학습 중인 인원은 최종 아트를 제작하기보다, 프로그래머가 바로 사용할 수 있는 임시 에셋, 애니메이션, 이펙트, 소켓, 블루프린트 연결 파이프라인을 담당한다.

## Day-by-Day Plan

### Day 1

목표: 프로젝트 골격과 캐릭터 이동 확인.

- Unreal 프로젝트 생성 또는 기존 프로젝트 정리
- Git 규칙, 브랜치 규칙, 커밋 규칙 확정
- 기본 테스트맵 생성
- Character, PlayerController, GameMode, GameState 골격 구성
- GAS 플러그인 활성화
- AbilitySystemComponent 기본 연결
- 로컬에서 캐릭터 1종 이동 확인

완료 기준:

- 캐릭터 하나가 테스트맵에서 움직인다.
- 기본 GameMode와 PlayerController 흐름이 잡혀 있다.

### Day 2

목표: 멀티플레이 접속과 팀 스폰 확인.

- Listen Server 또는 Dedicated Server 접속 테스트
- 2명 이상 접속 확인
- 팀 A/B 배정
- 팀별 PlayerStart 스폰
- TPS 카메라와 입력 확정
- Health, MaxHealth Attribute 추가

완료 기준:

- 두 클라이언트가 서로의 위치와 이동을 확인할 수 있다.
- 팀별 스폰 위치가 분리되어 있다.

### Day 3

목표: 서버 권한 기반 전투 확인.

- 기본 공격 GameplayAbility 구현
- Damage GameplayEffect 구현
- 서버 권한 피해 처리
- 피격 로그 또는 간단 UI 표시
- 사망 처리
- 임시 무기 또는 투사체 연결

완료 기준:

- 플레이어가 서로 공격해 체력을 줄일 수 있다.
- 체력이 0이 되면 사망 상태가 된다.

### Day 4

목표: AOS 승리 목표 확인.

- 타워 또는 넥서스 Actor 구현
- 구조물 Health 적용
- 구조물 피격/파괴 처리
- 팀별 공격 가능 여부 처리
- 넥서스 파괴 승패 판정

완료 기준:

- 구조물이 공격을 받아 파괴된다.
- 넥서스 파괴 시 승패가 판정된다.

### Day 5

목표: 한 판 테스트 가능한 1주차 빌드 정리.

- 2라인 블록아웃 맵 정리
- 리스폰 흐름 추가
- 간단 HUD: 체력, 팀, 승패
- 네트워크 테스트
- 주요 버그 수정
- 1주차 빌드 후보 생성

완료 기준:

- 2명 이상이 접속해 한 판을 끝까지 진행할 수 있다.
- 빌드 상태와 남은 이슈가 문서화되어 있다.

## Role Deliverables

### A. Lead / Network / Game Rules

- FieryTaleGameMode
- FieryTaleGameState
- 팀 배정
- 팀별 스폰
- Match Start / Match End
- Victory Condition
- 주간 빌드 안정화 판단

### B. GAS / Combat

- AttributeSet
- Health / MaxHealth
- Damage GameplayEffect
- BasicAttack GameplayAbility
- Death GameplayTag
- 서버 권한 피해 처리
- 전투 관련 디버그 로그

### C. TPS Character / Input

- 캐릭터 이동
- TPS 카메라
- 조준 방향
- 기본 공격 입력 바인딩
- 피격/사망 상태 연결
- 플레이 감각 1차 조정

### D. TA 1

- 임시 캐릭터 메시 연결
- 기본 Idle / Run / Attack 애니메이션 연결
- Animation Blueprint
- 피격 이펙트 자리 표시
- 무기 소켓
- 투사체 시작 위치 정리

### E. Planning + TA 2

- 2라인 테스트맵 블록아웃
- 넥서스/타워 위치
- 1주차 룰 문서
- QA 체크리스트
- 캐릭터 2종 초안
- 단, 1주차 구현은 공통 캐릭터 1종과 공통 기본 공격 1종에 집중

## Week 1 Quality Levels

### Good

- 4인 접속 가능
- 팀 자동 분리
- 기본 공격, 피해, 사망 가능
- 구조물 파괴와 승패 가능
- 블록아웃 맵에서 한 판 진행 가능

### Acceptable

- 2인 접속 가능
- 기본 공격 가능
- 구조물 피해 가능
- 승패는 로그 수준

### Risky

- 싱글플레이 이동만 가능
- GAS 구조만 있고 전투가 작동하지 않음
- 맵과 문서는 있으나 멀티플레이 검증이 안 됨

4주 프로젝트에서 1주차에 멀티플레이 전투 검증이 되지 않으면, 3주차와 4주차에 일정 리스크가 크게 증가한다.

## Priority

### Week 1 Must-Have

- 공통 캐릭터 1종
- 공통 기본 공격 1종
- Health / Damage / Death
- 팀 / 스폰 / 승패
- 타워 또는 넥서스
- 2라인 블록아웃 맵
- 서버 권한 기반 동작

### Week 1 Defer

- 캐릭터별 고유 스킬
- 미니언
- 성장 시스템
- 아이템
- 정교한 UI
- 최종 아트
- 고급 애니메이션
- 중립 오브젝트

## Inputs Needed for Next Assignment Pass

다음번에 다섯 명의 상세 스펙을 기반으로 작업 분배를 다시 하려면 아래 정보가 필요하다.

- 각 인원의 Unreal Engine 경험 수준
- C++ 경험 수준
- Blueprint 경험 수준
- GAS 경험 여부
- 멀티플레이/Replication 경험 여부
- UI/UMG 경험 여부
- 애니메이션 블루프린트 경험 여부
- 머티리얼, VFX, Niagara 경험 여부
- 레벨 블록아웃 경험 여부
- 기획 문서 작성 경험
- 주당 실제 투입 가능 시간
- 각자 선호하거나 피하고 싶은 작업

## Current Recommendation

1주차는 콘텐츠 확장보다 시스템 검증에 집중한다.

가장 중요한 판단 기준은 "첫 주에 한 판이 끝까지 돌아가는가"이다. 화면이 투박하더라도 팀 접속, 이동, 공격, 피해, 구조물 파괴, 승패 판정이 연결되면 2주차부터 캐릭터와 맵을 확장할 수 있다.
