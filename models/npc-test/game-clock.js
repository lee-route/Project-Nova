/**
 * Session-scoped game tick (UE GameTimeTick 대체). 월드 시간 경과·KB decay 동기화.
 */
(function (global) {
  var SESSION_STORE = new Map();
  var DEFAULT_TICK = 1000;

  function defaultClock() {
    return {
      tick: DEFAULT_TICK,
      day: 1,
      hour: 8,
      ticksPerHour: 10,
      ticksPerDay: 240,
      history: [],
    };
  }

  function getClock(sessionKey) {
    var key = String(sessionKey || "default");
    if (!SESSION_STORE.has(key)) {
      SESSION_STORE.set(key, defaultClock());
    }
    return SESSION_STORE.get(key);
  }

  function clearClock(sessionKey) {
    SESSION_STORE.delete(String(sessionKey || "default"));
  }

  function setTick(sessionKey, tick, meta) {
    var c = getClock(sessionKey);
    var t = Math.max(0, Math.floor(Number(tick)));
    c.tick = t;
    if (meta && meta.day != null) c.day = meta.day;
    if (meta && meta.hour != null) c.hour = meta.hour;
    c.history.push({ tick: t, reason: (meta && meta.reason) || "set", at: Date.now() });
    return snapshot(sessionKey);
  }

  function advance(sessionKey, deltaTicks, reason) {
    var c = getClock(sessionKey);
    var d = Math.max(0, Math.floor(Number(deltaTicks) || 0));
    if (d === 0) return snapshot(sessionKey);
    c.tick += d;
    var hoursPassed = Math.floor(d / c.ticksPerHour);
    if (hoursPassed > 0) {
      c.hour += hoursPassed;
      while (c.hour >= 24) {
        c.hour -= 24;
        c.day += 1;
      }
    }
    c.history.push({ tick: c.tick, delta: d, reason: reason || "advance", at: Date.now() });
    return snapshot(sessionKey);
  }

  function getTick(sessionKey) {
    return getClock(sessionKey).tick;
  }

  /**
   * executeScenario / quest-runtime 통합용 tick 해석
   */
  function resolveTick(overrides) {
    var o = overrides || {};
    if (o.currentTick != null && !isNaN(Number(o.currentTick))) {
      return Math.floor(Number(o.currentTick));
    }
    var sk = o.sessionKey || o.clockSessionKey;
    if (sk) return getTick(sk);
    return DEFAULT_TICK;
  }

  function afterPropagation(sessionKey, opts) {
    var o = opts || {};
    var advanceBy = Number(o.advanceTicksAfterPropagate);
    if (!isNaN(advanceBy) && advanceBy > 0) {
      return advance(sessionKey, advanceBy, o.reason || "after_propagation");
    }
    return snapshot(sessionKey);
  }

  function snapshot(sessionKey) {
    return JSON.parse(JSON.stringify(getClock(sessionKey)));
  }

  global.GameClock = {
    getClock: getClock,
    getTick: getTick,
    setTick: setTick,
    advance: advance,
    clearClock: clearClock,
    resolveTick: resolveTick,
    afterPropagation: afterPropagation,
    snapshot: snapshot,
  };
})(typeof window !== "undefined" ? window : globalThis);
