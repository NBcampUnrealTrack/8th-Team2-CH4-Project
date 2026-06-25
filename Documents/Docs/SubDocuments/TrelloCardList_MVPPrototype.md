# Trello Card List - MVP Prototype

## 목적

이 문서는 FieryTale 4주 프로토타입을 Trello 보드에 바로 등록하기 위한 카드 목록이다.

카드는 `TrelloTaskGuidelines.md`, `MVPScopeDecision.md`, `Week1_TeamPlan.md` 기준으로 작성했다. 우선순위는 2주 안에 한 판이 끝까지 돌아가는 MVP 완성이다.

## 보드 컬럼 추천

| 컬럼 | 용도 |
|---|---|
| Backlog | 전체 후보 카드 |
| This Week | 이번 주 목표 카드 |
| Ready | 선행 조건이 없어 바로 가져갈 수 있는 카드 |
| In Progress | 진행 중 |
| Review | 구현 후 검수/통합 대기 |
| Done | 동작 확인 완료 |
| Blocked | 진행 불가 |

## 라벨 추천

- `MVP`
- `BestGoal`
- `Polish`
- `Planning`
- `Character`
- `GAS`
- `Structure`
- `Network`
- `Lobby`
- `UI`
- `Data`
- `Map`
- `Chat`
- `Leaderboard`
- `QA`
- `Bug`

## 오늘 바로 추가할 상위 리스트

처음 보드를 만들 때는 아래 묶음으로 카드를 넣는다.

1. `Week 0 / Planning Lock`
2. `Week 1 / Core Playable Loop`
3. `Week 2 / MVP Match Complete`
4. `Week 3 / Presentation Features`
5. `Week 4 / Polish And Stabilization`
6. `Best Goal / Optional`

Trello 컬럼은 상태 기준으로 유지하고, 위 묶음명은 카드 제목 앞 프리픽스 또는 체크리스트 그룹명으로 쓰는 것을 권장한다.

---

## Week 0 / Planning Lock

### [W0] MVP 시점 결정 문서 작성

태그: `MVP`, `Planning`

목적: 4주 프로토타입에서 TPS 절충형을 유지할지, 낮은 쿼터뷰로 전환할지 결정한다.

작업 내용:
- `CameraViewpointComparison.md`와 `MVPScopeDecision.md` 비교
- 4주 일정 기준 리스크 정리
- 최종 추천안 1개 선택

완료 조건:
- `Docs/SubDocuments/MVPViewpointDecision.md` 작성
- 선택한 시점과 제외한 시점의 이유가 명확함
- 구현 영향이 Character, Camera, Map, Combat 기준으로 정리됨

확인 방법:
- 팀원이 문서만 보고 MVP 카메라 기준을 설명할 수 있음

관련 파일/블루프린트:
- `Docs/SubDocuments/CameraViewpointComparison.md`
- `Docs/SubDocuments/MVPScopeDecision.md`

주의 사항:
- 장기 비전보다 4주 안에 검증 가능한 범위를 우선한다.

### [W0] CoreMatchRules 문서 작성

태그: `MVP`, `Planning`, `Network`, `Structure`

목적: 한 판의 시작, 진행, 종료 규칙을 구현 가능한 수준으로 고정한다.

작업 내용:
- 팀 구성 규칙 정리
- 스폰, 사망, 리스폰 규칙 정리
- 타워/넥서스 피해 가능 조건 정리
- 승패 조건 정리

완료 조건:
- `Docs/SubDocuments/CoreMatchRules.md` 작성
- GameMode, GameState, PlayerState에 들어갈 책임이 구분됨
- 2주 MVP 제외 항목이 명시됨

확인 방법:
- 구현자가 문서 기준으로 클래스 책임을 나눌 수 있음

관련 파일/블루프린트:
- `Docs/SubDocuments/MVPScopeDecision.md`

주의 사항:
- 미니언, 아이템, 성장 시스템은 2주 MVP 규칙에 넣지 않는다.

### [W0] GASCombatPlanning 문서 작성

태그: `MVP`, `Planning`, `GAS`

목적: GAS 기반 전투 구현 전 Attribute, Ability, Effect, Tag 기준을 정한다.

작업 내용:
- Health/MaxHealth 등 1차 Attribute 선정
- 기본 공격, 공격 스킬, 이동 스킬, 궁극기 슬롯 정의
- Damage GameplayEffect 흐름 정의
- 최소 GameplayTag 목록 작성

완료 조건:
- `Docs/SubDocuments/GASCombatPlanning.md` 작성
- 1주차 구현에 필요한 GAS 요소가 표로 정리됨
- 서버 권한 피해 처리 기준이 포함됨

확인 방법:
- GAS 담당자가 문서 기준으로 첫 AttributeSet과 Ability를 만들 수 있음

관련 파일/블루프린트:
- `Docs/SubDocuments/MVPScopeDecision.md`

주의 사항:
- 복잡한 버프/디버프 설계보다 피해, 사망, 쿨다운 흐름을 우선한다.

### [W0] 2라인 테스트맵 블록아웃 규칙 정리

태그: `MVP`, `Planning`, `Map`, `Structure`

목적: 1주차 회색 박스 맵을 만들기 위한 최소 배치 기준을 정한다.

작업 내용:
- 팀별 스폰 위치 기준 정리
- 2라인 방향과 교전 거리 기준 정리
- 방어타워와 넥서스 배치 기준 정리
- 테스트용 엄폐물/시야 차단물 기준 정리

완료 조건:
- 라인 2개, 팀별 스폰, 타워, 넥서스 위치가 텍스트 또는 간단 도식으로 정리됨
- 첫 블록아웃에서 제외할 요소가 명시됨

확인 방법:
- 맵 담당자가 문서 기준으로 회색 박스 맵을 만들 수 있음

관련 파일/블루프린트:
- `ConceptArt/StorybookWorld_ConceptGuide.md`
- `Docs/SubDocuments/MVPScopeDecision.md`

주의 사항:
- 아트 콘셉트보다 플레이 흐름 검증이 먼저다.

---

## Week 1 / Core Playable Loop

### [W1] Unreal 프로젝트 기본 구조 확인

태그: `MVP`, `Network`, `Character`

목적: 구현을 시작할 수 있는 Unreal 프로젝트 기본 상태를 확인한다.

작업 내용:
- 프로젝트 실행 확인
- 기본 맵 실행 확인
- GameMode, PlayerController, Character 생성 기준 확인
- GAS 플러그인 사용 가능 여부 확인

완료 조건:
- 에디터에서 프로젝트가 열린다
- 빈 테스트맵 또는 기본 테스트맵이 실행된다
- GAS 플러그인 활성화 여부가 확인된다

확인 방법:
- 에디터 실행 후 PIE 1인 플레이 확인

관련 파일/블루프린트:
- Unreal Project

주의 사항:
- 프로젝트 생성 자체가 안 되어 있으면 이 카드는 즉시 `Blocked`로 표시한다.

### [W1] BaseCharacter 이동/카메라 입력 연결

태그: `MVP`, `Character`

목적: 플레이어가 테스트맵에서 캐릭터를 조작할 수 있게 한다.

작업 내용:
- 이동 입력 연결
- 카메라 회전 연결
- 점프 또는 기본 이동 상태 확인
- PlayerController와 Pawn possession 확인

완료 조건:
- 이동 입력이 동작한다
- 카메라 회전이 동작한다
- PIE에서 캐릭터 조작이 가능하다

확인 방법:
- PIE에서 1인 플레이로 이동/카메라 확인

관련 파일/블루프린트:
- `BaseCharacter`
- `PlayerController`

주의 사항:
- 최종 조작감보다 동작 확인을 우선한다.

### [W1] 멀티플레이 이동 복제 확인

태그: `MVP`, `Character`, `Network`

목적: 서버와 클라이언트에서 플레이어 위치가 서로 보이게 한다.

작업 내용:
- Listen Server PIE 설정 확인
- 클라이언트 2개 접속 확인
- 캐릭터 위치 복제 확인

완료 조건:
- 서버와 클라이언트가 같은 맵에 접속한다
- 서로의 캐릭터 이동이 보인다
- 위치가 심하게 어긋나지 않는다

확인 방법:
- PIE 2 players, Listen Server로 이동 복제 확인

관련 파일/블루프린트:
- `BaseCharacter`
- `GameMode`

주의 사항:
- 전투 구현 전에 반드시 확인한다.

### [W1] GameMode 팀 배정 구현

태그: `MVP`, `Network`, `Lobby`

목적: 접속한 플레이어를 Team A/B로 서버 권한 배정한다.

작업 내용:
- PlayerState에 TeamId 추가
- GameMode에서 접속 순서 또는 균등 방식으로 팀 배정
- 팀 정보 복제 확인

완료 조건:
- 접속한 플레이어가 Team A/B 중 하나를 가진다
- 클라이언트에서 자신의 팀을 확인할 수 있다
- 2명 접속 시 서로 다른 팀으로 배정된다

확인 방법:
- PIE 2 clients에서 로그 또는 임시 UI로 TeamId 확인

관련 파일/블루프린트:
- `FieryTaleGameMode`
- `FieryTalePlayerState`

주의 사항:
- 클라이언트가 팀을 직접 결정하지 않는다.

### [W1] 팀별 PlayerStart 스폰 구현

태그: `MVP`, `Network`, `Map`

목적: 팀에 따라 서로 다른 위치에 스폰되게 한다.

작업 내용:
- Team A/B PlayerStart 배치
- GameMode에서 팀별 스폰 위치 선택
- 리스폰에도 같은 규칙을 재사용할 수 있게 정리

완료 조건:
- Team A는 A 진영에 스폰된다
- Team B는 B 진영에 스폰된다
- 2인 접속 시 시작 위치가 분리된다

확인 방법:
- PIE 2 clients에서 각자 다른 진영 스폰 확인

관련 파일/블루프린트:
- `GameMode`
- `PlayerStart`
- Test Map

주의 사항:
- 맵 완성도보다 위치 분리와 재현성을 우선한다.

### [W1] BaseCharacter에 AbilitySystemComponent 연결

태그: `MVP`, `Character`, `GAS`

목적: 캐릭터가 GAS Ability와 Attribute를 사용할 수 있게 한다.

작업 내용:
- ASC 추가
- ASC 초기화 위치 정리
- 서버/클라이언트에서 ASC 유효성 확인
- 기본 Ability 부여 테스트

완료 조건:
- ASC가 생성된다
- 서버와 클라이언트에서 ASC가 유효하다
- 기본 Ability 부여 로그가 확인된다

확인 방법:
- PIE에서 로그로 ASC 초기화와 Ability 부여 확인

관련 파일/블루프린트:
- `BaseCharacter`
- `AbilitySystemComponent`

주의 사항:
- 복잡한 Ability보다 ASC 연결 안정성을 우선한다.

### [W1] AttributeSet에 Health/MaxHealth 추가

태그: `MVP`, `GAS`

목적: 피해와 사망 처리를 위한 기본 체력 Attribute를 만든다.

작업 내용:
- Health 추가
- MaxHealth 추가
- 초기값 적용
- Health 변경 로그 추가

완료 조건:
- Health와 MaxHealth가 존재한다
- 초기값이 적용된다
- Health 변경을 로그 또는 임시 UI로 확인할 수 있다

확인 방법:
- PIE에서 Attribute 초기값 로그 확인

관련 파일/블루프린트:
- `FieryTaleAttributeSet`

주의 사항:
- 마나, 방어력 등은 첫 카드에 포함하지 않는다.

### [W1] Damage GameplayEffect 생성

태그: `MVP`, `GAS`

목적: 공격이 Health를 감소시키는 공통 피해 효과를 만든다.

작업 내용:
- Damage GameplayEffect 생성
- Health 감소 연결
- 피해량 임시값 설정
- 서버 적용 흐름 확인

완료 조건:
- Damage Effect 적용 시 Health가 감소한다
- 피해량 임시값이 조정 가능하다
- 서버에서 피해 적용이 확인된다

확인 방법:
- 테스트 입력 또는 디버그 호출로 Health 감소 확인

관련 파일/블루프린트:
- `GE_Damage`
- `AttributeSet`

주의 사항:
- 치명타, 방어력, 피해 타입은 제외한다.

### [W1] 기본 공격 Ability 입력 바인딩

태그: `MVP`, `Character`, `GAS`

목적: 플레이어 입력으로 기본 공격 Ability를 실행한다.

작업 내용:
- 기본 공격 입력 설정
- Ability 슬롯 연결
- Ability 실행 로그 추가
- 쿨다운 임시 처리

완료 조건:
- 입력 시 기본 공격 Ability가 실행된다
- 서버에서 Ability 실행이 확인된다
- 연속 입력 시 최소한의 중복 실행 방지가 있다

확인 방법:
- PIE에서 입력 후 로그 확인

관련 파일/블루프린트:
- `GA_BasicAttack`
- `BaseCharacter`

주의 사항:
- 타격 판정은 별도 카드로 분리한다.

### [W1] 기본 공격 피해 판정 구현

태그: `MVP`, `GAS`, `Network`

목적: 기본 공격으로 적 플레이어 또는 구조물에 피해를 준다.

작업 내용:
- 공격 대상 판정 구현
- Damage GameplayEffect 적용
- 아군 대상 피해 방지
- 구조물 대상 피해 연결

완료 조건:
- 적 플레이어 Health가 감소한다
- 아군에게 피해가 들어가지 않는다
- 구조물에도 피해를 줄 수 있다

확인 방법:
- PIE 2 clients에서 서로 공격 테스트

관련 파일/블루프린트:
- `GA_BasicAttack`
- `GE_Damage`
- `BaseCharacter`
- `TowerBase`

주의 사항:
- 정확한 조준 보정은 폴리싱 단계로 미룬다.

### [W1] 사망 상태 전환 구현

태그: `MVP`, `Character`, `GAS`, `Network`

목적: Health가 0이 되면 캐릭터가 사망 상태로 전환되게 한다.

작업 내용:
- Health 0 감지
- Dead GameplayTag 적용
- 이동/공격 입력 차단
- 사망 로그 또는 임시 UI 표시

완료 조건:
- Health 0에서 사망 상태가 된다
- 사망 중 공격이 실행되지 않는다
- 사망 상태가 클라이언트에 복제된다

확인 방법:
- PIE 2 clients에서 공격 후 사망 상태 확인

관련 파일/블루프린트:
- `BaseCharacter`
- `AttributeSet`
- `GameplayTag.State.Dead`

주의 사항:
- 사망 애니메이션은 필수 범위가 아니다.

### [W1] 고정 리스폰 구현

태그: `MVP`, `Character`, `Network`

목적: 사망한 플레이어가 고정 시간 뒤 팀 스폰 지점에서 부활한다.

작업 내용:
- 사망 후 5초 타이머 적용
- 팀별 스폰 위치로 이동
- Health 회복
- 조작 가능 상태 복구

완료 조건:
- 사망 후 고정 시간 뒤 부활한다
- 팀 스폰 지점에서 부활한다
- Health가 MaxHealth로 회복된다

확인 방법:
- PIE 2 clients에서 사망 후 리스폰 확인

관련 파일/블루프린트:
- `GameMode`
- `BaseCharacter`
- `PlayerStart`

주의 사항:
- 리스폰 시간 증가 규칙은 제외한다.

### [W1] TowerBase 체력/팀/피해 수신 구현

태그: `MVP`, `Structure`, `Network`

목적: 구조물이 서버 권한으로 피해를 받을 수 있게 한다.

작업 내용:
- TeamId 추가
- Health/MaxHealth 추가
- 피해 수신 함수 구현
- 아군 공격 방지

완료 조건:
- TowerBase가 팀과 체력을 가진다
- 적 공격을 받으면 서버에서 체력이 감소한다
- 아군 공격은 무시된다

확인 방법:
- 클라이언트 공격으로 서버 구조물 체력 감소 확인

관련 파일/블루프린트:
- `TowerBase`

주의 사항:
- 자동 공격 기능은 포함하지 않는다.

### [W1] TowerBase 파괴 상태 구현

태그: `MVP`, `Structure`, `Network`

목적: 구조물 체력이 0이 되면 파괴 상태로 전환한다.

작업 내용:
- Health 0 감지
- Destroyed 상태 복제
- 충돌 또는 피격 가능 상태 변경
- 파괴 이벤트 브로드캐스트

완료 조건:
- 체력 0에서 파괴 상태가 된다
- 파괴 상태가 클라이언트에 보인다
- 파괴 이벤트가 다른 시스템에서 받을 수 있다

확인 방법:
- 공격으로 TowerBase 파괴 후 로그 확인

관련 파일/블루프린트:
- `TowerBase`

주의 사항:
- 파괴 연출은 폴리싱 카드로 분리한다.

### [W1] 테스트 Nexus Actor 생성

태그: `MVP`, `Structure`

목적: 승패 목표로 사용할 넥서스 구조물을 만든다.

작업 내용:
- 팀별 Nexus 배치
- Health/MaxHealth 적용
- 기본 피해 수신 연결
- 파괴 이벤트 연결 준비

완료 조건:
- 팀별 Nexus가 맵에 존재한다
- Nexus가 체력을 가진다
- 적 공격으로 피해를 받을 수 있다

확인 방법:
- PIE에서 Nexus 체력 감소 로그 확인

관련 파일/블루프린트:
- `Nexus`
- `TowerBase`

주의 사항:
- 취약 상태 조건은 별도 카드로 처리한다.

### [W1] Nexus 파괴 승패 로그 구현

태그: `MVP`, `Structure`, `Network`, `UI`

목적: 넥서스 파괴 시 서버 권한으로 승패가 판정되게 한다.

작업 내용:
- Nexus 파괴 이벤트를 GameMode로 전달
- 승리 팀/패배 팀 판정
- GameState에 매치 결과 저장
- 로그 또는 임시 UI 출력

완료 조건:
- Nexus 파괴 시 매치 결과가 정해진다
- 서버에서 승패가 판정된다
- 클라이언트에서 승패 로그 또는 임시 UI를 볼 수 있다

확인 방법:
- PIE에서 Nexus 파괴 후 승패 로그 확인

관련 파일/블루프린트:
- `GameMode`
- `GameState`
- `Nexus`

주의 사항:
- 정식 승패 화면은 Week 2 또는 Polish 카드로 분리한다.

### [W1] 2라인 회색 박스 테스트맵 제작

태그: `MVP`, `Map`, `Structure`

목적: 한 판 흐름을 테스트할 수 있는 최소 전장을 만든다.

작업 내용:
- 팀별 스폰 영역 배치
- 2개 라인 구성
- 팀별 Tower/Nexus 배치
- 기본 충돌 확인

완료 조건:
- 양 팀 스폰 위치가 있다
- 라인 2개가 있다
- 팀별 Tower 또는 Nexus가 배치되어 있다
- 캐릭터가 이동 가능한 경로가 있다

확인 방법:
- PIE에서 양 팀 스폰 후 라인 이동 확인

관련 파일/블루프린트:
- Test Map
- `PlayerStart`
- `TowerBase`
- `Nexus`

주의 사항:
- 시각 완성도는 목표가 아니다.

### [W1] 임시 HUD 체력/팀 표시

태그: `MVP`, `UI`

목적: 테스트 중 플레이어 상태를 화면에서 확인할 수 있게 한다.

작업 내용:
- Health 표시
- TeamId 표시
- Dead/Alive 표시
- 임시 텍스트 UI 구성

완료 조건:
- 자신의 Health가 표시된다
- 자신의 TeamId가 표시된다
- 사망 상태가 표시된다

확인 방법:
- PIE에서 공격 후 Health 변화 확인

관련 파일/블루프린트:
- HUD Widget
- `PlayerState`
- `AttributeSet`

주의 사항:
- 최종 UI 디자인은 제외한다.

### [W1] Week 1 통합 플레이 테스트

태그: `MVP`, `QA`, `Network`

목적: 1주차 핵심 루프가 끊기지 않는지 확인한다.

작업 내용:
- 2인 접속
- 팀 스폰
- 이동
- 기본 공격
- 피해/사망/리스폰
- 구조물 파괴
- 승패 로그 확인

완료 조건:
- 최소 2인 접속으로 한 판 흐름을 끝까지 확인한다
- 실패한 항목이 이슈 카드로 분리된다
- 1주차 빌드 상태가 기록된다

확인 방법:
- 체크리스트 기반 수동 테스트

관련 파일/블루프린트:
- 전체 MVP 핵심 시스템

주의 사항:
- 실패 항목을 숨기지 말고 `Bug` 또는 `Blocked` 카드로 만든다.

---

## Week 2 / MVP Match Complete

### [W2] 기본 로비 맵/화면 생성

태그: `MVP`, `Lobby`, `UI`

목적: 매치 시작 전 플레이어가 대기할 수 있는 최소 로비를 만든다.

완료 조건:
- 로비 화면이 열린다
- 접속한 플레이어가 로비 상태에 머문다
- 매치 시작 전 인게임 스폰이 되지 않는다

확인 방법:
- 서버 접속 후 로비 진입 확인

### [W2] 로비 플레이어 목록 표시

태그: `MVP`, `Lobby`, `UI`, `Network`

목적: 접속한 플레이어와 팀 정보를 로비에서 확인한다.

완료 조건:
- 접속 플레이어 ID가 표시된다
- 팀 정보가 표시된다
- 플레이어 퇴장 시 목록이 갱신된다

확인 방법:
- 클라이언트 2개 접속 후 목록 확인

### [W2] 로비 준비 상태 구현

태그: `MVP`, `Lobby`, `Network`, `UI`

목적: 플레이어가 준비 완료 상태를 표시할 수 있게 한다.

완료 조건:
- 플레이어별 Ready 상태가 있다
- Ready 상태가 클라이언트에 복제된다
- 로비 UI에서 Ready 상태가 보인다

확인 방법:
- 2인 접속 후 Ready 토글 확인

### [W2] 매치 시작 버튼 구현

태그: `MVP`, `Lobby`, `Network`

목적: 로비에서 서버 권한으로 매치를 시작한다.

완료 조건:
- 호스트 또는 서버 권한에서 매치 시작 가능
- 플레이어가 인게임 맵 또는 스폰 상태로 전환된다
- 팀별 스폰이 적용된다

확인 방법:
- 로비에서 매치 시작 후 인게임 진입 확인

### [W2] 캐릭터 Data Table 구조 생성

태그: `MVP`, `Data`, `Character`, `GAS`

목적: 캐릭터 선택에 필요한 데이터를 Data Table로 관리한다.

완료 조건:
- 캐릭터 ID, 표시 이름, 역할군, 클래스, Attribute, Ability 필드가 있다
- 최소 2개 테스트 캐릭터 데이터가 있다
- 로비에서 데이터 로딩이 가능하다

확인 방법:
- 로비 UI 또는 로그로 2개 데이터 로딩 확인

### [W2] 로비 캐릭터 선택 UI 구현

태그: `MVP`, `Lobby`, `UI`, `Data`

목적: 플레이어가 로비에서 테스트 캐릭터를 선택할 수 있게 한다.

완료 조건:
- 캐릭터 목록이 표시된다
- 캐릭터를 선택할 수 있다
- 선택한 캐릭터 ID가 저장된다

확인 방법:
- 서로 다른 캐릭터 선택 후 PlayerState 값 확인

### [W2] 선택 캐릭터 ID를 PlayerState에 저장

태그: `MVP`, `Lobby`, `Data`, `Network`

목적: 캐릭터 선택 결과를 서버 기준 상태로 저장한다.

완료 조건:
- 선택 캐릭터 ID가 PlayerState에 저장된다
- 클라이언트에서 같은 선택 상태를 볼 수 있다
- 매치 시작 시 선택 데이터를 참조할 수 있다

확인 방법:
- 2인 접속 후 각자 선택값 복제 확인

### [W2] 선택한 캐릭터 데이터로 스폰

태그: `MVP`, `Lobby`, `Data`, `Network`, `Character`

목적: 로비 선택 결과가 인게임 Pawn 스폰에 반영되게 한다.

완료 조건:
- 선택 캐릭터 ID에 따라 Pawn 또는 Ability 구성이 달라진다
- 서버 기준으로 스폰된다
- 클라이언트에서도 동일하게 보인다

확인 방법:
- 서로 다른 캐릭터 2종 선택 후 스폰 확인

### [W2] 공격 스킬 Ability 슬롯 구현

태그: `MVP`, `GAS`, `Character`

목적: 기본 공격 외 공격 스킬 슬롯 실행을 검증한다.

완료 조건:
- 공격 스킬 입력이 있다
- Ability가 실행된다
- 쿨다운 또는 중복 실행 방지가 있다

확인 방법:
- PIE에서 공격 스킬 입력 후 로그/피해 확인

### [W2] 이동 스킬 Ability 슬롯 구현

태그: `MVP`, `GAS`, `Character`, `Network`

목적: 짧은 대시 또는 회피형 이동 스킬 실행을 검증한다.

완료 조건:
- 이동 스킬 입력이 있다
- 서버 기준 위치 변경이 적용된다
- 클라이언트에서 이동 결과가 보인다

확인 방법:
- PIE 2 clients에서 이동 스킬 사용 확인

### [W2] 궁극기 Ability 슬롯 구현

태그: `MVP`, `GAS`, `Character`

목적: 긴 쿨다운을 가진 상위 스킬 슬롯을 검증한다.

완료 조건:
- 궁극기 입력이 있다
- Ability가 실행된다
- 긴 쿨다운 또는 재사용 제한이 있다

확인 방법:
- PIE에서 궁극기 입력 후 실행/쿨다운 확인

### [W2] 스킬 쿨다운 HUD 표시

태그: `MVP`, `UI`, `GAS`

목적: 전투 입력 4종의 사용 가능 상태를 화면에 표시한다.

완료 조건:
- 기본 공격 상태가 표시된다
- 공격 스킬 쿨다운이 표시된다
- 이동 스킬 쿨다운이 표시된다
- 궁극기 쿨다운이 표시된다

확인 방법:
- 각 스킬 사용 후 HUD 변화 확인

### [W2] 타워 파괴 시 넥서스 취약 상태 전환

태그: `MVP`, `Structure`, `Network`

목적: 방어타워 중 하나가 파괴되면 해당 팀 넥서스를 공격 가능하게 한다.

완료 조건:
- 타워 파괴 이벤트가 발생한다
- 해당 팀 넥서스가 Vulnerable 상태가 된다
- 취약 전에는 넥서스 피해가 막힌다
- 취약 후에는 넥서스 피해가 적용된다

확인 방법:
- 타워 파괴 전/후 넥서스 피해 가능 여부 확인

### [W2] 넥서스 상태 HUD 표시

태그: `MVP`, `UI`, `Structure`

목적: 넥서스 체력과 취약 상태를 플레이어가 확인할 수 있게 한다.

완료 조건:
- 아군/적 넥서스 체력이 표시된다
- 넥서스 취약 상태가 표시된다
- 넥서스 파괴 상태가 표시된다

확인 방법:
- 타워 파괴 전/후 HUD 상태 확인

### [W2] 방어타워 상태 HUD 표시

태그: `MVP`, `UI`, `Structure`

목적: 양 팀 방어타워 상태를 플레이어가 확인할 수 있게 한다.

완료 조건:
- 타워 생존/파괴 상태가 표시된다
- 가능하면 타워 체력이 표시된다
- 타워 파괴 시 HUD가 갱신된다

확인 방법:
- 타워 파괴 후 HUD 변화 확인

### [W2] 승패 메시지 UI 구현

태그: `MVP`, `UI`, `Network`

목적: 매치 종료 결과를 모든 플레이어에게 보여준다.

완료 조건:
- 승리 팀에는 Victory 메시지가 표시된다
- 패배 팀에는 Defeat 메시지가 표시된다
- 매치 종료 후 추가 입력 또는 공격이 제한된다

확인 방법:
- Nexus 파괴 후 양쪽 클라이언트 UI 확인

### [W2] 매치 재시작 또는 종료 흐름 구현

태그: `MVP`, `Lobby`, `Network`

목적: 한 판 종료 후 다음 테스트로 넘어갈 수 있게 한다.

완료 조건:
- 매치 종료 후 로비로 돌아가거나 맵을 재시작할 수 있다
- 플레이어 상태가 초기화된다
- 다음 판에서 팀/스폰/체력이 정상이다

확인 방법:
- 한 판 종료 후 재시작 테스트

### [W2] 2주 MVP 통합 테스트

태그: `MVP`, `QA`, `Network`

목적: 로비부터 넥서스 파괴 승패까지 2주 MVP 흐름을 검증한다.

완료 조건:
- 서버 또는 방 접속
- 로비 진입
- 플레이어 목록/팀/캐릭터 선택 확인
- 매치 시작
- 전투, 타워 파괴, 넥서스 파괴, 승패 메시지 확인
- 실패 항목이 Bug 카드로 분리됨

확인 방법:
- 최소 2인, 가능하면 4인 수동 테스트

---

## Week 3 / Presentation Features

### [W3] 전체 채팅 송수신 구현

태그: `Polish`, `Chat`, `Network`, `UI`

완료 조건:
- 플레이어가 채팅 메시지를 보낼 수 있다
- 모든 클라이언트가 메시지를 받는다
- 메시지에 발신자 표시가 있다

### [W3] 채팅창에 킬로그 표시

태그: `Polish`, `Chat`, `UI`

완료 조건:
- 플레이어 사망 시 채팅창에 알림이 표시된다
- 공격자/피해자 정보가 표시된다

### [W3] 채팅창에 구조물 알림 표시

태그: `Polish`, `Chat`, `Structure`, `UI`

완료 조건:
- 타워 파괴 알림이 표시된다
- 넥서스 취약화 알림이 표시된다
- 넥서스 파괴 알림이 표시된다

### [W3] TAB 리더보드 열기/닫기

태그: `Polish`, `Leaderboard`, `UI`

완료 조건:
- TAB 입력으로 리더보드가 열린다
- TAB 해제 또는 재입력으로 닫힌다
- 기본 플레이 조작과 충돌하지 않는다

### [W3] 리더보드에 팀/킬/데스 표시

태그: `Polish`, `Leaderboard`, `Network`, `UI`

완료 조건:
- 플레이어 이름 또는 임시 ID가 표시된다
- 팀이 표시된다
- 킬/데스가 표시된다
- 사망/킬 발생 시 값이 갱신된다

### [W3] 로비 화면 가독성 개선

태그: `Polish`, `Lobby`, `UI`

완료 조건:
- 플레이어 목록, 팀, 준비 상태가 보기 쉽게 정리된다
- 캐릭터 선택 상태가 명확하다
- 매치 시작 조건을 이해할 수 있다

### [W3] 매치 시작/종료 UX 개선

태그: `Polish`, `Lobby`, `UI`

완료 조건:
- 매치 시작 전 카운트다운 또는 안내가 있다
- 매치 종료 후 결과를 확인할 시간이 있다
- 다음 행동이 명확하다

### [W3] 파생 테스트 캐릭터 1종 추가

태그: `BestGoal`, `Character`, `GAS`, `Data`

완료 조건:
- Data Table에 신규 캐릭터가 추가된다
- 기존 BaseCharacter 기반으로 스폰된다
- Attribute 또는 Ability 구성이 기존 캐릭터와 다르다

주의 사항:
- MVP 안정화가 끝난 뒤에만 진행한다.

---

## Week 4 / Polish And Stabilization

### [W4] 공격 적중 피드백 추가

태그: `Polish`, `Character`, `GAS`

완료 조건:
- 공격 적중 시 시각 또는 사운드 피드백이 있다
- 빗나감과 적중을 구분할 수 있다

### [W4] 피격 피드백 추가

태그: `Polish`, `Character`, `UI`

완료 조건:
- 피격 시 화면 또는 캐릭터 피드백이 있다
- Health 감소를 체감할 수 있다

### [W4] 사망/리스폰 피드백 개선

태그: `Polish`, `Character`, `UI`

완료 조건:
- 사망 상태가 명확히 보인다
- 리스폰 남은 시간이 표시된다
- 부활 시 조작 가능 상태가 명확하다

### [W4] 구조물 파괴 피드백 추가

태그: `Polish`, `Structure`, `UI`

완료 조건:
- 타워 파괴가 시각/로그/UI로 명확히 전달된다
- 넥서스 취약화가 명확히 전달된다

### [W4] 승패 화면 가독성 개선

태그: `Polish`, `UI`

완료 조건:
- 승패 결과가 크게 표시된다
- 아군이 승리했는지 패배했는지 즉시 알 수 있다
- 결과 화면이 너무 빨리 사라지지 않는다

### [W4] 기본 사운드 연결

태그: `Polish`

완료 조건:
- 기본 공격 사운드가 있다
- 피격 사운드가 있다
- 타워/넥서스 파괴 사운드가 있다
- 승패 사운드가 있다

### [W4] 2 vs 2 접속 안정성 테스트

태그: `MVP`, `QA`, `Network`

완료 조건:
- 4인 접속이 가능하다
- 팀이 2 vs 2로 나뉜다
- 한 판을 끝까지 진행할 수 있다
- 발생한 이슈가 Bug 카드로 등록된다

### [W4] 반복 플레이 안정성 테스트

태그: `Polish`, `QA`, `Bug`

완료 조건:
- 같은 세션에서 3판 이상 반복 테스트한다
- 재시작 후 팀/체력/스폰 상태가 정상이다
- 크래시 또는 치명적 진행 불가 이슈가 기록된다

### [W4] 최종 시연 체크리스트 작성

태그: `MVP`, `QA`, `Planning`

완료 조건:
- 시연 시작 방법이 정리된다
- 호스트/클라이언트 접속 순서가 정리된다
- 반드시 보여줄 기능 목록이 있다
- 알려진 이슈와 회피 방법이 적혀 있다

---

## Best Goal / Optional

### [BG] Dedicated Server 빌드 생성

태그: `BestGoal`, `Network`

완료 조건:
- Dedicated Server 빌드가 생성된다
- 서버 프로세스가 실행된다
- 실행 로그가 확인된다

주의 사항:
- Listen Server 2 vs 2가 안정화되기 전에는 진행하지 않는다.

### [BG] Dedicated Server 클라이언트 접속 확인

태그: `BestGoal`, `Network`, `QA`

완료 조건:
- 클라이언트가 Dedicated Server에 접속한다
- 최소 1 vs 1 매치 진입이 가능하다
- 가능하면 2 vs 2 접속을 확인한다

주의 사항:
- 실패해도 4주 필수 목표 실패로 보지 않는다.

### [BG] 단순 미니언 웨이브 구현 검토

태그: `BestGoal`, `Structure`, `Network`

완료 조건:
- 미니언 추가 여부가 결정된다
- 추가한다면 근거리 미니언 1종만 우선 구현 범위로 정한다
- 경로 이동, 타겟팅, 공격 판정 리스크가 정리된다

주의 사항:
- MVP 카드가 남아 있으면 진행하지 않는다.

### [BG] 라인별 시스템 알림 추가

태그: `BestGoal`, `UI`, `Chat`

완료 조건:
- 어느 라인의 타워가 파괴되었는지 표시된다
- 넥서스 취약화 원인이 표시된다

### [BG] 캐릭터별 콘셉트 스킬 1개 적용

태그: `BestGoal`, `Character`, `GAS`, `Data`

완료 조건:
- 원전 캐릭터 1명의 상징을 반영한 스킬이 있다
- 기존 Ability 구조를 깨지 않는다
- Data Table 기반으로 선택 가능하다

주의 사항:
- 콘텐츠 매력 검증용이며, 시스템 안정화보다 우선하지 않는다.

---

## 오늘 Trello에 먼저 넣을 Ready 카드

오늘 바로 작업 배분까지 할 수 있는 카드는 아래 10개다.

1. `[W0] MVP 시점 결정 문서 작성`
2. `[W0] CoreMatchRules 문서 작성`
3. `[W0] GASCombatPlanning 문서 작성`
4. `[W0] 2라인 테스트맵 블록아웃 규칙 정리`
5. `[W1] Unreal 프로젝트 기본 구조 확인`
6. `[W1] BaseCharacter 이동/카메라 입력 연결`
7. `[W1] GameMode 팀 배정 구현`
8. `[W1] BaseCharacter에 AbilitySystemComponent 연결`
9. `[W1] TowerBase 체력/팀/피해 수신 구현`
10. `[W1] 2라인 회색 박스 테스트맵 제작`

## 오늘은 Backlog에만 넣고 아직 시작하지 말 카드

아래 카드는 선행 조건이 있으므로 Backlog에 넣되, 바로 Ready로 올리지 않는다.

- `[W1] 기본 공격 피해 판정 구현`
- `[W1] 사망 상태 전환 구현`
- `[W1] 고정 리스폰 구현`
- `[W1] Nexus 파괴 승패 로그 구현`
- `[W2] 선택한 캐릭터 데이터로 스폰`
- `[W2] 타워 파괴 시 넥서스 취약 상태 전환`
- `[W2] 승패 메시지 UI 구현`
- 모든 `W3`, `W4`, `BG` 카드

## 현재 기획상 주의할 점

- 2주 MVP에서는 미니언, 아이템, 성장 시스템, 미니맵을 넣지 않는다.
- Listen Server 기준 2 vs 2 시연 가능 상태가 필수 목표다.
- Dedicated Server는 `BestGoal`로 둔다.
- 구조물 자동 공격은 MVP 필수가 아니다.
- 캐릭터 수를 늘리는 것보다 `BaseCharacter`와 Data Table 기반 확장이 우선이다.
- 카드 완료 기준은 "작성/구현"이 아니라 "PIE 또는 플레이 테스트에서 동작 확인"이다.
