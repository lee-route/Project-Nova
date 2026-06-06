# Unreal Engine ↔ npc-test API (v1)

**UE 팀 1순위 문서:** `../INTEGRATION-GUIDE.txt` (동일 내용 + Blueprint 체크리스트·트러블슈팅)

`nova/` 프로젝트에는 아직 C++/Blueprint 연동 코드가 없습니다.  
v1은 **로컬 Node API 서버**에 HTTP로 붙이는 방식을 권장합니다.

## 서버 기동

```bash
cd models/npc-test
npm run api
```

상세: `../API-QUICKSTART.md`

## 권한·세이브

- **Authoritative save: UE** — gold, `worldFlags`, 퀘스트 진행은 언리얼 세이브게임이 소유합니다.
- API `sessionKey`는 **평판·서버 RAM 데모**용입니다. 서버 재시작 시 초기화됩니다.
- 필요 시 `GET /v1/session/snapshot` / `POST /v1/session/import`로 dev 복원만 사용하세요.

## v1에서 쓰는 엔드포인트 (2+1)

| Method | Path | 용도 |
|--------|------|------|
| POST | `/v1/quest/turn-in` | 플레이어 보고 → 분기·보상 |
| GET | `/v1/quest/accept-dialogue?giverId=&sessionKey=` | 수락 대사 (선택) |
| POST | `/v1/parse` | 디버그·사전 검증 (선택) |

**v1에 없음:** LLM 대사, `runQuestFlow` 전체, travel/interact (UE 맵에서 처리).

## UE Blueprint 흐름 (권장)

1. 플레이어가 조사·이동 완료 (UE)
2. 보고 UI에서 문자열 수집 → `scenarioText`
3. `HTTP POST` `http://127.0.0.1:8787/v1/quest/turn-in`

```json
{
  "questId": "quest_abandoned_cargo",
  "giverId": "guard_timid",
  "scenarioText": "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다",
  "sessionKey": "player-save-slot-1"
}
```

`giverId`는 **퀘스트를 준 의뢰인**입니다 (`GET /v1/quest/meta` 참고).  
학자 알릭은 보고 **수신** 쪽(`turnInProfileKey`)이며, `giverId`와 다를 수 있습니다.

4. 응답에서 UE가 반영할 필드:

- `outcome.rewards.gold`
- `outcome.worldFlags.cargo_destination` (`authority` | `black_market` | `unknown`)
- `completion.completed`
- `outcomeBranch.branchId`

5. `knowledgeAudit` — **판정에 사용하지 않음** (v1 진단만). 기만·조사 패널티는 API v2.

## investigation → WorldTruth (선택)

조사 완료 시 UE가 알고 있는 객관 사실을 같이 보낼 수 있습니다:

```json
{
  "worldTruthFacts": [
    { "subject": "마약초", "quantity": 12, "is_countable": true, "action_type": "inventory", "target": "안개 계곡" }
  ]
}
```

`knowledgeAudit.worldCompare`에만 반영되며, v1에서는 gold 분기를 바꾸지 않습니다.

## 오류 형식

```json
{ "ok": false, "error": { "code": "missing_giver_id", "message": "...", "details": null } }
```

## 상세 스펙

`../GAME-API.json` (version 2)
