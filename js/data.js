/**
 * FireGuard Data Layer
 * 
 * ESP32 exposes: GET /api/sensors
 * Response: { flame, pump_active, cooldown_left, flame_events, uptime, heap, rssi, alert_level, timestamp }
 * 
 * Set USE_MOCK_DATA: false and update API_BASE_URL with your ESP32's IP.
 */

const CONFIG = {
  API_BASE_URL:  'http://192.168.1.105/api',
  POLL_INTERVAL: 2000,    // ms — how often to poll ESP32
  USE_MOCK_DATA: true,    // flip to false when ESP32 is connected
};

// ── Mock Simulation ───────────────────────────────────────────────────────────
const MockSensor = (() => {
  let t = 0;
  let fireCountdown = 80;    // triggers a demo fire event every ~80 ticks
  let pumpCooldown  = 0;
  let flameEvents   = 0;
  let lastFlameAt   = 0;

  function next() {
    t++;
    fireCountdown--;

    const flame = fireCountdown > 0 && fireCountdown <= 12;

    if (flame && fireCountdown === 12) {
      flameEvents++;
      lastFlameAt = t;
    }
    if (fireCountdown <= 0) {
      fireCountdown = 80 + Math.floor(Math.random() * 60);
    }

    // Pump stays on 10 ticks after flame gone
    if (flame) pumpCooldown = 10;
    else if (pumpCooldown > 0) pumpCooldown--;

    const pumpActive   = flame || pumpCooldown > 0;
    const cooldownLeft = !flame && pumpCooldown > 0 ? pumpCooldown : 0;

    return {
      flame,
      pump_active:    pumpActive,
      cooldown_left:  cooldownLeft,
      flame_events:   flameEvents,
      alert_level:    flame ? 2 : (pumpActive ? 1 : 0),
      uptime:         t * (CONFIG.POLL_INTERVAL / 1000),
      heap:           200 - Math.floor(t * 0.01),
      rssi:           -52 - Math.floor(Math.random() * 10),
      last_flame_sec: lastFlameAt > 0 ? (t - lastFlameAt) : null,
      timestamp:      Date.now(),
    };
  }

  return { next };
})();

// ── Real API fetch ─────────────────────────────────────────────────────────────
async function fetchSensorData() {
  if (CONFIG.USE_MOCK_DATA) {
    return new Promise(resolve => setTimeout(() => resolve(MockSensor.next()), 120));
  }

  try {
    const res = await fetch(`${CONFIG.API_BASE_URL}/sensors`, {
      signal: AbortSignal.timeout(5000),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.warn('[FireGuard] API error:', err.message);
    return null;
  }
}
