# 로컬 전용 대용량 에셋 (Git 미포함)

GitHub LFS 용량 한도 때문에 아래 에셋은 **원격 저장소에 올리지 않습니다**.  
팀원은 **`nova 게임 압축본/`** (또는 팀 공유 드라이브)에서 프로젝트 `Content/` 아래에 복사해 사용하세요.

## 미포함 폴더 (~8,000+ 파일, 약 12GB+)

| 폴더 | 용도 |
|------|------|
| `Content/ParagonTerra/` | 플레이어 캐릭터 Terra (Paragon) |
| `Content/ParagonSevarog/` | 보스 Sevarog |
| `Content/ParagonGrux/` | 보스 Grux |
| `Content/ParagonKwang/` | 보스 Kwang |
| `Content/ParagonRampage/` | 보스 Rampage 등 |
| `Content/ROG_Creatures/` | 몬스터(Stickman 등) |
| `Content/Whisper/` | Whisper 맵·에셋 |
| `Content/DemoTemplate/` | UE 데모 템플릿 |
| `Content/ai/` | `BP_NewMonster` 등 |
| `Content/__ExternalActors__/DemoTemplate/` | 데모 맵 외부 액터 |
| `Content/__ExternalActors__/Variant_Horror/` | 호러 변형 맵 액터 |
| `Content/__ExternalActors__/Variant_Shooter/` | 슈터 변형 맵 액터 |
| `Content/__ExternalObjects__/DemoTemplate/` | 데모 외부 오브젝트 |
| `Content/__ExternalObjects__/Variant_Horror/` | 호러 외부 오브젝트 |
| `Content/__ExternalObjects__/Variant_Shooter/` | 슈터 외부 오브젝트 |

## Git에 포함된 것 (main)

- **C++ / 설정:** `Source/NovaUproject/`, `Config/`, `NovaUproject.uproject`
- **통합 스크립트:** `Scripts/apply_shared_project_patches.py`
- **던전 팩:** `Content/Fantastic_Dungeon_Pack/` (규민 브랜치 merge 기준, LFS)
- **플레이어 BP:** `Content/FirstPerson/Blueprints/` (`BP_FirstPersonCharacter` 등)
- **입력·UI 패치:** `Content/Input/IMC_Default.uasset`, `WBP_InteractPrompt.uasset` (F키 대화 문구)
- **로컬라이제이션:** `Content/Localization/` (대화 문자열)

## 로컬 셋업 순서

```bash
git checkout main
git pull origin main
git lfs pull   # 던전·FirstPerson 등 포함 에셋만
```

1. `nova 게임 압축본`에서 위 **미포함 Paragon/ROG/Whisper** 폴더를 `Content/`에 복사
2. (선택) `python Scripts/apply_shared_project_patches.py` — F키 상호작용 등 BP 패치
3. `NovaUproject.uproject` 실행

## 참고

- `Config/LocalNovaVoice.ini` — Azure 음성 키 (항상 로컬 전용)
- `*.uasset.bak` — 패치 백업 (Git 제외)
