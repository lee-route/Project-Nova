# npc-test — 정보 왜곡·퀘스트 프로토타입

정식 퀘스트: **quest_abandoned_cargo** (밀수업자의 버려진 화물)

언리얼 연동은 **포함하지 않음**. 브라우저·Node로 엔진·퀘스트·평판·로컬 게임 상태까지 검증합니다.

## 빠른 시작

```powershell
cd models\npc-test
node run-all-tests.mjs
.\run-api.ps1            # API 서버 (8787) — npm 불필요
.\run-api-smoke.ps1      # HTTP 연동 확인 (서버 실행 중)
```

npm 사용 가능 시: `npm run api`, `npm run api:smoke`, `npm run api:e2e`

상세: [API-QUICKSTART.md](API-QUICKSTART.md)

브라우저: `index.html` 을 정적 서버로 연 뒤 Quest Playtest 패널 사용.

언리얼: **[INTEGRATION-GUIDE.txt](INTEGRATION-GUIDE.txt)** (UE 팀 1순위) — 세이브는 UE, API는 turn-in·분기만.  
요약: [ue-bridge/INTEGRATION.md](ue-bridge/INTEGRATION.md) · 서버 실행: [API-QUICKSTART.md](API-QUICKSTART.md)

## 주요 파일

| 파일 | 역할 |
|------|------|
| `npc-parser.js` | 한국어 시나리오 → `facts[]` (Node/UE 서버용, DOM 없음) |
| `dictionaries.js` | 파서·분류 lexicon (`NpcLexicon`) |
| `quest-system.js` | 왜곡·전파·KB |
| `quest-runtime.js` | 퀘스트 turn-in, flow, processSteps |
| `quest-game-state.js` | gold / worldFlags / history (로컬) |
| `quests-draft.json` | 퀘스트 데이터 |
| `npcs.json` | NPC 왜곡 프로필 |
| `player-profile.json` | 플레이어 + 초기 평판 |
| `reputation-config.json` | tier 의뢰 대사 |
| `npc-api-server.mjs` | HTTP API v1 (`npm run api`) |
| `GAME-API.json` | API v2 스펙 (facts, propagation, errors) |
| `ue-bridge/INTEGRATION.md` | UE HTTP 연동 절차 |

## 퀘스트 플레이 (브라우저)

1. **Accept Quest** — 의뢰인별 neutral 대사 + 브리핑
2. **Next Step** — dialogue → travel → fact_input(보고)
3. **Run Full Flow** — 위를 자동 + 게임 상태 반영
4. **Game State** — `gold`, `worldFlags.cargo_destination` 등

## API (JS)

- `QuestRuntime.getAcceptDialogue(questId, giverId, { sessionKey })`
- `QuestRuntime.acceptQuest` / `advanceProcessStep` / `runQuestFlow`
- `QuestRuntime.runQuestTurnIn({ questId, giverId, scenarioText, engine, parser })`
- `QuestGameState.snapshot(sessionKey)` / `exportState`

상세: `ENGINE-DESIGN.txt`, `GAME-API.json`
