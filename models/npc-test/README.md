# npc-test — 정보 왜곡·퀘스트 프로토타입

정식 퀘스트: **quest_abandoned_cargo** (밀수업자의 버려진 화물)

언리얼 연동은 **포함하지 않음**. 브라우저·Node로 엔진·퀘스트·평판·로컬 게임 상태까지 검증합니다.

## 빠른 시작

```bash
cd models/npc-test
node run-all-tests.mjs
```

브라우저: `index.html` 을 정적 서버로 연 뒤 Quest Playtest 패널 사용.

## 주요 파일

| 파일 | 역할 |
|------|------|
| `quest-system.js` | 왜곡·전파·KB |
| `quest-runtime.js` | 퀘스트 turn-in, flow, processSteps |
| `quest-game-state.js` | gold / worldFlags / history (로컬) |
| `quests-draft.json` | 퀘스트 데이터 |
| `npcs.json` | NPC 왜곡 프로필 |
| `player-profile.json` | 플레이어 + 초기 평판 |
| `reputation-config.json` | tier 의뢰 대사 |
| `GAME-API.json` | 향후 UE 연동용 JSON 계약 (브릿지 없음) |

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
