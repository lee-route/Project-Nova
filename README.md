# Project-Nova 팀 가이드

> UE 5.7 · 기본 브랜치 **`main`**  
> 음성·상쇄(C++) + 던전/NPC 에셋(규민 import) 통합 상태 기준

---

## 1. 받기 & 실행

```bash
git checkout main
git pull origin main
git lfs pull
```

- **`git lfs pull` 필수** — 안 하면 `.uasset` / `.umap`이 포인터만 남아 맵·BP가 깨집니다.
- **실행:** 루트 `NovaUproject.uproject`
- **기본 맵:** `/Game/ThirdPerson/Lvl_ThirdPerson`
- **GameMode:** `NovaGameMode` → `BP_NovaPlayerController`

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

## 3. 통합 현황

| 구분 | 내용 |
|------|------|
| **음성·상쇄·무기** | C++ STT/파서, `NovaGameMode`, `BP_NovaPlayerController`, Hammer enum(BP 호환) |
| **던전·NPC** | `Content/Fantastic_Dungeon_Pack/` — 맵 5개, `bp_npc`, 대화 UI 등 |
| **로컬라이제이션** | `Config/Localization/` |
| **Git 제외** | `Config/LocalNovaVoice.ini`, `_local-only/`, `_import_tmp/` |

### ParagonSevarog 미포함 (중요)

`Content/ParagonSevarog/`는 **아직 import하지 않았습니다.**

- 던전 맵 **1, 2, 3, 5** → `/Game/ParagonSevarog` 참조 → **Missing Reference** 가능
- 맵 **4** (`map_dungeon_level_4_temple`)는 Paragon 참조 없음 → 상대적으로 안전

던전 맵에서 **음성·QWER**를 쓰려면 **World Settings → GameMode Override**를 `NovaGameMode`로 맞추세요.

---

## 4. 역할 분담

| C++ (완료·유지) | 블루프린트 (연출) |
|-----------------|-------------------|
| 마이크 → STT → 명령 파싱 | 무기 메시·몽타주 |
| 무기 **상태**, 상쇄 **판정** | 상쇄 VFX, 스킬 이펙트 |
| `OpenBossCounterWindow()` API | `OnSecondaryWeaponChanged` 등 이벤트 연출 |

`BP_NovaPlayerController`는 `NovaClickMovePlayerController` **자식**이어야 음성이 동작합니다.

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
| `1~4` | 검 / 활 / 낫 / 방패 |
| `Q~R` | 현재 무기 스킬 |
| `F5~F8` | 레이저 / 공간절단 / 소환 / 잔해 상쇄 윈도우 |
| 음성 | `방패`, `활`, `낫`, `검` |

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
3. GameMode = `NovaGameMode`, PC = `BP_NovaPlayerController`  
4. 던전 Missing → ParagonSevarog import 필요 여부  
5. BP enum 오류 → `ENovaVoiceCommand`: **Hammer = 검** (DisplayName `Sword`)  
6. C++ 빌드 실패 → 에디터 종료 후 빌드, 또는 **Ctrl+Alt+F11**

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
| `NovaClickMovePlayerController.*` | 음성·키보드·`OpenBossCounterWindow` |
| `NovaCombatVoiceGateComponent.*` | 상쇄 윈도우·판정 |
| `NovaVoiceCaptureComponent.*` | 마이크·VAD·STT |
| `NovaVoiceCommandParser.*` | 명령 파싱 |
| `Config/DefaultNovaVoice.ini` | 기본 음성 설정 |

**NPC·퀘스트 API 프로토타입:** `models/npc-test/` (별도 문서)
