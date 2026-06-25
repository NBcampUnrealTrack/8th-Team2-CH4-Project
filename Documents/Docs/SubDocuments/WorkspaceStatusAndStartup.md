# Workspace Status And Startup

## 목적

이 문서는 Codex가 `FieryTale` 폴더에서 새로 실행되었을 때, 폴더 전체 상태를 빠르게 파악하고 바로 다음 기획 작업으로 들어가기 위한 시작점이다.

`AGENTS.md`는 작업 규칙이고, 이 문서는 현재 산출물 상태와 다음 액션을 정리하는 운영용 인덱스다.

## 현재 폴더 구조 요약

| 위치 | 상태 | 용도 |
|---|---|---|
| `AGENTS.md` | 존재 | Codex 작업 규칙과 우선순위 |
| `Docs/InitialConcept.md` | 존재 | 프로젝트 기본 기획, 장르, MVP 방향 |
| `Docs/SubDocuments/` | 존재 | 시점, MVP 범위, 코어 매치 규칙, 5인 작업 분류, 성장 구조, 주차별 계획 등 세부 기획 |
| `Docs/FairyTales/` | 존재 | 원전 동화/설화 후보와 선정 기준 |
| `Docs/Character/` | 존재 | 캐릭터 후보별 콘셉트 문서 |
| `ConceptArt/` | 존재 | 배경/캐릭터 콘셉트 가이드와 이미지 산출물 |
| `Dev/` | 존재 | 개발 준비 문서와 프로토타입 참고 산출물 |
| `Calendar/` | 존재 | 일정 템플릿 |
| `DeveloperStats/` | 존재 | 개발자 역량 또는 통계 참고 자료 |

## 우선 읽기 순서

새 작업 시작 시 아래 순서로 읽는다.

1. `AGENTS.md`
2. `Docs/InitialConcept.md`
3. `Docs/SubDocuments/WorkspaceStatusAndStartup.md`
4. `Docs/SubDocuments/MVPScopeDecision.md`
5. `Docs/SubDocuments/CameraViewpointComparison.md`
6. `Docs/SubDocuments/MVPViewpointDecision.md`
7. `Docs/SubDocuments/CoreMatchRules.md`
8. `Docs/SubDocuments/FivePersonWorkCategories.md`
9. `Docs/SubDocuments/Week1_TeamPlan.md`
10. `ConceptArt/StorybookWorld_ConceptGuide.md`

캐릭터, 원전, 세계관 소재를 다루는 작업이면 추가로 다음을 확인한다.

- `Docs/FairyTales/00_SelectionCriteria.md`
- 관련 `Docs/FairyTales/*.md`
- 관련 `Docs/Character/*.md`
- `ConceptArt/CharacterConceptSheetGuide.md`

## 현재 결정 요약

| 항목 | 현재 기준 |
|---|---|
| 장르 축 | TPS 팀 전투와 단순 AOS 목표의 절충형 |
| MVP 시점 | TPS 확정. 캐릭터 뒤쪽 위에서 보는 TPS 카메라를 기준으로 한다. |
| 초기 매치 규모 | 2 vs 2 |
| 2주 MVP 최소 검증 | 최소 1 vs 1로도 한 판 완료 가능 |
| 서버 구조 | Listen Server 필수, Dedicated Server는 선택 목표 |
| 권한 구조 | 피해, 사망, 구조물 파괴, 승패는 서버 권한 |
| 전장 | 2라인, 팀별 넥서스 1개, 팀별 1단계 방어타워 |
| 미니언 | 2주 MVP 제외, 3주차 이후 선택 확장 |
| 성장 | 1차 MVP에서는 성장 구조 제거 권장 |
| 코어 매치 규칙 | 로비, 팀 배정, 스폰, 사망, 5초 리스폰, 타워 파괴, 넥서스 취약화, 넥서스 파괴 승패 흐름 작성 완료 |
| 5인 작업 분류 | 네트워크/매치 흐름, 캐릭터/TPS 조작, GAS/전투, 전장/구조물, UI/로비/데이터/QA로 분류 완료 |
| 캐릭터 기반 | `BaseCharacter` 1종과 GAS Ability 슬롯 구조 우선 |
| 스킬 입력 | 기본 공격, 공격 스킬, 이동 스킬, 궁극기 |
| 구조물 | `TowerBase` 공통 구조, 방어타워와 넥서스 확장 가능하게 설계 |
| 승패 | 방어타워 중 하나 파괴 후 넥서스 공격 가능, 넥서스 파괴 시 종료 |
| UI | 체력, 스킬 쿨다운, 팀, 구조물 상태, 승패 메시지 우선 |
| 제외 항목 | 미니맵, 정교한 매치메이킹, 아이템, 복잡한 성장, 정교한 밸런스 |

## 현재 열린 질문

| 질문 | 현재 판단 |
|---|---|
| MVP 시점은 TPS인가, 쿼터뷰인가? | `MVPViewpointDecision.md`에서 TPS로 확정했다. |
| 한 매치 목표 시간은 몇 분인가? | 미정. 4주 프로토타입 기준 8~12분 후보 검토가 필요하다. |
| 캐릭터 역할군을 어떻게 나눌 것인가? | 미정. 전통 역할군보다 2 vs 2 기능 역할 중심 검토가 필요하다. |
| 초기 캐릭터 2종은 누구인가? | 미정. Data Table 검증용 테스트 캐릭터와 최종 후보를 분리해야 한다. |
| GAS 태그/속성 명명 규칙은? | 미정. 구현 착수 전 최소 규칙 문서가 필요하다. |
| 저작권/퍼블릭 도메인 기준은? | 미정. 원전 후보 문서와 연결한 관리 기준이 필요하다. |

## 가장 가까운 다음 액션

구체적인 사용자 지시가 없으면 다음 순서로 진행한다.

1. `Docs/SubDocuments/MVPViewpointDecision.md` 작성
   - 작성 완료. MVP 시점은 TPS로 확정한다.
2. `Docs/SubDocuments/CoreMatchRules.md` 작성
   - 작성 완료. 팀, 스폰, 로비, 매치 시작, 사망, 리스폰, 구조물, 승패 규칙을 구현 가능한 규칙으로 고정했다.
3. `Docs/SubDocuments/FivePersonWorkCategories.md` 작성
   - 작성 완료. 5명 작업자를 위한 큰 작업 분류와 책임 범위를 정리했다.
4. `Docs/SubDocuments/GASCombatPlanning.md` 작성
   - Attribute, Ability, GameplayEffect, GameplayTag, 서버 권한 판정 기준을 초안으로 정리한다.
5. `Docs/SubDocuments/InitialCharacterRoles.md` 작성
   - 2 vs 2 기준 역할군과 초기 2종 캐릭터 후보를 정리한다.
6. `Docs/SubDocuments/PrototypeBacklog.md` 작성
   - 4주 프로토타입 기능 백로그, 우선순위, 담당 영역, 완료 기준을 표로 정리한다.

## 시작 시 확인할 명령

Codex는 새 작업 시작 시 아래 성격의 확인을 먼저 수행한다.

```powershell
git status --short
rg --files
```

필요하면 세부 폴더를 추가로 확인한다.

```powershell
Get-ChildItem -Recurse -Force Docs
Get-ChildItem -Recurse -Force ConceptArt
Get-ChildItem -Recurse -Force Dev
```

## 문서 갱신 기준

이 문서는 다음 상황에서 갱신한다.

- 새 핵심 기획 문서가 추가되었을 때
- MVP 범위, 시점, 성장, 전장, 캐릭터 역할군 중 하나가 확정되었을 때
- 폴더 구조나 작업 우선순위가 바뀌었을 때
- 다음 Codex 실행자가 별도 설명 없이 이어가기 어렵다고 판단될 때

## 다음 Codex에게 남기는 작업 지시

현재는 기획 산출물 정리가 우선이다.

MVP 시점, 코어 매치 규칙, 5인 작업 분류는 문서화가 완료되었다.

다음으로는 `GASCombatPlanning.md`를 작성해 실제 구현자가 AttributeSet, GameplayAbility, GameplayEffect, GameplayTag, 서버 권한 피해 판정으로 바로 분해할 수 있게 만든다. 동시에 Trello `ThisWeek`에는 `FivePersonWorkCategories.md` 기준으로 1주차 핵심 카드만 이동하는 것이 좋다.
