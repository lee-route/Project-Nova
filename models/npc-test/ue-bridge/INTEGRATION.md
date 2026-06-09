# Unreal Engine ↔ npc-test API (v1)

**UE LLM 1페이지:** `../UE-LLM-GUIDE.txt`  
**UE 전체:** `../INTEGRATION-GUIDE.txt`  
**API 상세:** `../API-QUICKSTART.md`

`nova/` 프로젝트에는 아직 C++/Blueprint 연동 코드가 없습니다.  
v1은 **로컬 Node API 서버**에 HTTP로 붙이는 방식을 권장합니다.

## 서버 기동

```bash
cd models/npc-test
npm run api
```

## 권한·세이브

- **Authoritative save: UE** — gold, `worldFlags`, 퀘스트 진행은 언리얼 세이브게임이 소유합니다.
- API `sessionKey`는 **평판·서버 RAM 데모**용입니다. 서버 재시작 시 초기화됩니다.

## v1 엔드포인트

| Method | Path | 용도 |
|--------|------|------|
| POST | `/v1/quest/turn-in` | 보고 → 분기·보상 **+ LLM 대사** (`dialogue.npcSpeech`) |
| GET | `/v1/quest/accept-dialogue?giverId=&sessionKey=` | 수락 대사 (선택) |
| POST | `/v1/dialogue` | LLM 단독 테스트 (UE는 turn-in만 권장) |
| POST | `/v1/parse` | 디버그 (선택) |

## UE Blueprint 흐름 (권장)

1. 플레이어 조사·이동 (UE)
2. 보고 UI → `scenarioText`
3. **`HTTP POST` `/v1/quest/turn-in`** (한 번)

```json
{
  "questId": "quest_abandoned_cargo",
  "giverId": "guard_timid",
  "scenarioText": "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  "sessionKey": "player-save-slot-1"
}
```

4. 응답 처리:

| 필드 | UE 동작 |
|------|---------|
| `completion.completed` | `false` → 실패 UI, 보상 금지 |
| `outcome.rewards.gold` | SaveGame |
| `outcome.worldFlags.cargo_destination` | SaveGame |
| `dialogue.npcSpeech` | 알릭 대화 UI / 자막 |
| `dialogue.provider` | `openai` / `mock` (디버그) |

5. LLM 생략: `"includeDialogue": false` in request body.

## LLM 설정

서버 PC에서 `llm-config.local.json` 또는 env (`OPENAI_API_KEY`, `NOVA_LLM_USE_LIVE=true`).  
키 없으면 **mock** 대사 (비용 없음). `GET /v1/llm/status` 확인.

## 상세 스펙

`../GAME-API.json`
