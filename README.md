# Project-Nova 팀 가이드

> UE 5.7 · 기본 브랜치 **`main`**  
> **현진(음성·조작)** + **nova 게임 압축본(보스·NPC·Paragon 에셋)** 통합

---

## 1. 받기 & 실행

```bash
git checkout main
git pull origin main
git lfs pull
```

- **`git lfs pull`** — Git에 포함된 던전·FirstPerson 등 LFS 에셋만 받습니다.
- **Paragon·ROG·Whisper 등 대용량 에셋은 Git에 없습니다.** → [`docs/LOCAL_ASSETS.md`](docs/LOCAL_ASSETS.md) 참고, `nova 게임 압축본/`에서 `Content/`로 복사하세요.
- **실행:** 루트 `NovaUproject.uproject`
- **기본 맵:** `/Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon` (던전 1 — 공유 프로젝트와 동일)
- **GameMode:** `NovaGameMode` → `BP_NovaPlayerController` (C++ `NovaClickMovePlayerController` 자식) + `BP_FirstPersonCharacter` (Terra)
- **NPC Cast:** `bp_npc`의 `BP_FirstPersonPlayerController` 참조는 `DefaultEngine.ini` **CoreRedirects**로 `BP_NovaPlayerController`에 리맵

**ThirdPerson 테스트:** Content Browser → `/Game/ThirdPerson/Lvl_ThirdPerson` — 음성·클릭 이동·탑다운 카메라 확인용.

**던전 플레이:** `NovaUproject.uproject` 실행 시 던전 1이 자동 로드됩니다. World Settings에서 GameMode Override가 `NovaGameMode`인지 확인하세요.

---

## 2. Azure 음성 설정

키는 Git에 없습니다. `Config/LocalNovaVoice.ini.example`을 복사해 `Config/LocalNovaVoice.ini`를 로컬에만 만듭니다.

```ini
[/Script/NovaUproject.NovaVoiceSettings]
AzureSubscriptionKey=Key1_붙여넣기
AzureRegion=koreacentral
AzureLanguage=ko-KR
```

- Region: `koreacentral` · Key 1 사용 (Key 2는 백업)
- PIE 후 화면에 `Voice: listening` 표시되면 정상
- 테스트 음성: `방패`, `활`, `낫`, `검`, `도와줘`

**마이크 안 될 때:** Windows 입력을 **Hands-Free / AG Audio**로 선택 (Stereo는 마이크 없음).  
`LocalNovaVoice.ini`에 `CaptureDeviceName=Hands-Free` 등 부분 이름 지정 가능.  
그래도 안 되면 키보드 `1~4` / `F5~F8`로 테스트.

---

## 3. 통합 현황 (압축본 merge)

| 구분 | 출처 | 내용 |
|------|------|------|
| **음성·상쇄·조작** | 현진 (우선) | C++ STT, `NovaGameMode`, `NovaClickMovePlayerController`, 무기·스킬·상쇄 키 |
| **던전·NPC·대화** | 압축본 | `Fantastic_Dungeon_Pack/`, `BP_FirstPersonCharacter`, `bp_npc`, `WBP_Dialogue` |
| **보스·몬스터** | 압축본 (**로컬만**) | `ParagonSevarog`, `ParagonGrux`, `ParagonKwang`, `ParagonRampage`, `ParagonTerra` — [`docs/LOCAL_ASSETS.md`](docs/LOCAL_ASSETS.md) |
| **기타 에셋** | 압축본 (**로컬만**) | `ROG_Creatures`, `Whisper`, `Content/ai/BP_NewMonster`, `DemoTemplate` |
| **Git 제외** | — | `LocalNovaVoice.ini`, `_local-only/`, `_import_tmp/`, `nova 게임 압축본/`, Paragon·ROG 등 ([`docs/LOCAL_ASSETS.md`](docs/LOCAL_ASSETS.md)) |

### 조작 (고정)

| 입력 | 동작 |
|------|------|
| **우클릭 (RMB)** | 클릭 이동 (홀드 시 목적지 갱신) |
| **좌클릭 (LMB)** | 대시 |
| **Space** | 점프 |
| **Q / W / E / R** | 스킬 |
| **A / D** (홀드) | 시점 좌/우 회전 |
| **F** | NPC 대화 시작 |
| **G** | NPC 대화 강제 종료 |

**음성·디버그 (유지):** `1~4` 무기, `F5~F8` 보스 상쇄 테스트, 음성 명령

Pawn Enhanced Input는 Possess 시 **비활성화** — 위 키는 `NovaClickMovePlayerController`에서만 처리합니다.
**ThirdPerson:** `BP_NovaPlayerController` 경로도 동일 C++ 기반으로 유지.

---

## 4. 역할 분담

| C++ (완료·유지) | 블루프린트 (연출) |
|-----------------|-------------------|
| 마이크 → STT → 명령 파싱 | 무기 메시·몽타주 |
| 무기 **상태**, 상쇄 **판정** | 상쇄 VFX, 스킬 이펙트 |
| `OpenBossCounterWindow()` API | `OnSecondaryWeaponChanged` 등 이벤트 연출 |

`BP_FirstPersonPlayerController` / `BP_NovaPlayerController` 모두 `NovaClickMovePlayerController` **자식**이어야 음성·대화가 동작합니다.

---

## 5. 보스 패턴 팀 — 연동

패턴 **텔레그래프 / 공격 시작 시점**에 한 번 호출:

```
OpenBossCounterWindow(패턴타입)
```

호출 주체: `NovaClickMovePlayerController` (`BlueprintCallable`)

| 보스 패턴 | 호출 값 | 플레이어 무기 |
|-----------|---------|---------------|
| 레이저 | `LaserShield` | 방패 |
| 공간절단 | `SpaceScythe` | 낫 |
| 소환 | `SummonBow` | 활 |
| 잔해 | `DebrisHammer` | 검 |

**자동 동작 (C++):** 약 1.2초 상쇄 윈도우 → 올바른 무기 음성/키 입력 시 성공 판정 → `OnBossCounterVisualSuccess` (연출은 BP).  
윈도우 길이: `Config/DefaultNovaVoice.ini` → `CounterWindowSeconds`

**보스팀이 할 일:** 위 API 호출만. Azure·STT·파서 설정은 불필요.

---

## 6. BP 연출 이벤트

| BP 이벤트 | 시점 |
|-----------|------|
| `OnSecondaryWeaponChanged` | 무기 전환 (음성·키보드) |
| `OnBossCounterVisualSuccess` | 상쇄 성공 직후 |
| `OnCompanionHelpVisualRequested` | "도와줘" 인식 |
| `BP_UseSkillQ/W/E/R` | Q~R 스킬 (무기별 연출) |

---

## 7. 테스트 키

| 입력 | 동작 |
|------|------|
| **RMB** | 클릭 이동 |
| **A / D** | 시점 회전 |
| **Q / W / E / R** | 스킬 |
| **Space** | 대시 |
| **F** | NPC 대화 |
| **G** | 대화 종료 |
| `1~4` | 검 / 활 / 낫 / 방패 |
| `F5~F8` | 상쇄 테스트 |
| 음성 | `방패`, `활`, `낫`, `검`, `도와줘` |

**상쇄 스모크:** F5 누른 뒤 `4`(방패) 또는 "방패" → `Boss counter success`면 정상.

**화면 메시지**

| 메시지 | 의미 |
|--------|------|
| `Counter window open: N` | 윈도우 열림 |
| `Boss counter success` | 상쇄 성공 |
| `Counter window missed` | 시간 내 입력 없음 |

---

## 8. 남은 작업

| 우선 | 작업 |
|------|------|
| 1 | PIE 음성 스모크 + VAD/신뢰도 튜닝 |
| 2 | 보스 패턴 → `OpenBossCounterWindow()` 연결 |
| 3 | 4패턴 × 음성 상쇄 통합 테스트 |
| 4 | BP: 무기·상쇄·QWER 연출 (`DrawSkillDebug` 등) |

보스 연동은 **F5~F8 + 1~4**로 음성 없이 먼저 검증 가능.

---

## 9. 문제 해결

1. `git lfs pull` 했는지  
2. `LocalNovaVoice.ini` / 마이크 권한  
3. GameMode = `NovaGameMode`, PC = `BP_NovaPlayerController` (던전·ThirdPerson 공통 C++ 기반)  
4. NPC 대화 안 됨 → **F** 키, CoreRedirects가 `DefaultEngine.ini`에 있는지 확인  
5. `BP_FirstPersonPlayerController` 로드 크래시 → `.bak`에서 복원, uasset reparent 패치 금지  
6. 던전 맵에서 조작/음성 안 됨 → **GameMode Override = `NovaGameMode`** 저장했는지  
7. Output Log `Failed to load` → `git lfs pull` 재실행  
8. BP enum 오류 → `ENovaVoiceCommand`: **Hammer = 검** (DisplayName `Sword`)  
9. C++ 빌드 실패 → **Crash Reporter·에디터 완전 종료** 후 빌드, 또는 **Ctrl+Alt+F11**

---

## 10. Git (자주 쓰는 명령)

```bash
# 작업 올리기
git add .
git commit -m "메모"
git push origin main

# 다른 브랜치 내용 가져오기
git pull origin main

# main에 브랜치 합치기
git checkout main
git merge 현진
git push origin main
```

---

## 관련 소스

| 파일 | 역할 |
|------|------|
| `NovaGameMode.cpp` | 던전 기본 Pawn/PC (`BP_FirstPerson*`) |
| `NovaClickMovePlayerController.*` | 음성·키보드·NPC 대화·`OpenBossCounterWindow` |
| `Scripts/apply_shared_project_patches.py` | BP reparent, E→F, IMC 복사 (merge 후 1회 실행) |
| `NovaCombatVoiceGateComponent.*` | 상쇄 윈도우·판정 |
| `NovaVoiceCaptureComponent.*` | 마이크·VAD·STT |
| `NovaVoiceCommandParser.*` | 명령 파싱 |
| `Config/DefaultNovaVoice.ini` | 기본 음성 설정 |

**NPC·퀘스트 API 프로토타입:** `models/npc-test/` (별도 문서)
