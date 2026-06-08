# Node API 연동 — 빠른 시작

## 1. 서버 실행

**Windows — `npm` 오류 / PATH 미갱신 (가장 확실)**

Node는 설치됐는데 **예전 터미널**이면 `npm`이 계속 안 됩니다. 아래 중 하나:

```cmd
cd models\npc-test
run-api.cmd
```

또는 PowerShell:

```powershell
cd models\npc-test
.\run-api.ps1
```

**같은 터미널에서 `npm` 쓰려면** (재시작 대신):

```powershell
. .\refresh-path.ps1
npm run api
```

Cursor/VS Code는 **터미널 패널 닫기 → 새 터미널** 또는 **Developer: Reload Window** 후 `npm run api`.

또는 Node만 PATH에 있으면:

```powershell
node npc-api-server.mjs
```

**npm이 설치된 경우**

```bash
cd models/npc-test
npm run api
```

> `npm`을 인식하지 못하면 [nodejs.org](https://nodejs.org)에서 **LTS** 설치 후 **새 터미널**을 여세요.  
> Cursor 내장 터미널만 쓰는 경우 `.\run-api.ps1`이 Cursor 번들 `node.exe`를 찾아 씁니다.

기본 주소: `http://127.0.0.1:8787`

다른 PC/에디터에서 UE 테스트 시:

```bash
node npc-api-server.mjs --host=0.0.0.0 --port=8787
```

## 2. 헬스 체크

브라우저 또는 curl:

```
GET http://127.0.0.1:8787/health
```

## 3. 퀘스트 메타 (의뢰인 목록)

```
GET http://127.0.0.1:8787/v1/quest/meta
```

`giverId`는 **의뢰인** (`guard_timid`, `merchant_greedy`, `scholar_alric`).  
보고 **수신** NPC는 응답의 `turnInProfileKey`(대부분 `scholar_alric`).

## 4. Turn-in (UE 핵심)

PowerShell:

```powershell
$body = @{
  questId = "quest_abandoned_cargo"
  giverId = "guard_timid"
  scenarioText = "안개 계곡에 밀수 화물이 버려져 있다. 마약초 열두 개가 들어 있다"
  sessionKey = "ue-player-1"
} | ConvertTo-Json -Compress

Invoke-RestMethod -Uri "http://127.0.0.1:8787/v1/quest/turn-in" -Method POST -Body $body -ContentType "application/json; charset=utf-8"
```

UE에서 반영할 필드:

- `completion.completed`
- `outcomeBranch.branchId` — `authority_path` | `black_market_path` | `fallback`
- `outcome.rewards.gold`
- `outcome.worldFlags.cargo_destination`

## 5. 자동 테스트

```bash
# 핸들러 단위 (서버 불필요)
npm run test:api

# 서버 떠 있는 상태에서 HTTP
npm run api:smoke

# 서버 기동 + smoke 한 번에
npm run api:e2e
```

## 6. 문서

- 스펙: `GAME-API.json`
- UE: `ue-bridge/INTEGRATION.md`
- 설계: `ENGINE-DESIGN.txt` §13
