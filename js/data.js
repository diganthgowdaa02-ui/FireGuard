/**
 * FireGuard Data Layer
 * Handles communication with the ESP32 API.
 * Replace API_BASE_URL with your ESP32's IP address.
 * The ESP32 should expose: GET /api/sensors
 */

const CONFIG = {
  API_BASE_URL: 'http://192.168.1.105/api',
  POLL_INTERVAL: 3000,          // ms between sensor polls
  TEMP_WARN: 50,                // °C warning threshold
  TEMP_DANGER: 60,              // °C danger threshold
  SMOKE_WARN: 200,              // ppm warning threshold
  SMOKE_DANGER: 300,            // ppm danger threshold
  USE_MOCK_DATA: true,          // set false when ESP32 is connected
};

// ── Mock data simulation ──────────────────────────────────────────────────────
// Simulates realistic sensor fluctuation when USE_MOCK_DATA is true.
const MockSensor = (() => {
  let temp = 27, smoke = 85, humidity = 55, t = 0;
  // Occasionally simulate a fire event for demo purposes
  let fireEventCountdown = 120;

  function next() {
    t++;
    fireEventCountdown--;

    // Simulate a brief fire event every ~2 minutes
    if (fireEventCountdown <= 0) {
      fireEventCountdown = 200 + Math.floor(Math.random() * 100);
    }

    const fireEvent = fireEventCountdown < 15;

    temp = fireEvent
      ? 65 + Math.random() * 15
      : 24 + Math.sin(t * 0.05) * 4 + Math.random() * 2;

    smoke = fireEvent
      ? 400 + Math.random() * 200
      : 60 + Math.sin(t * 0.03) * 20 + Math.random() * 15;

    humidity = 45 + Math.sin(t * 0.02) * 10 + Math.random() * 3;

    return {
      temperature: parseFloat(temp.toFixed(1)),
      smoke: Math.round(smoke),
      humidity: parseFloat(humidity.toFixed(1)),
      flame: fireEvent,
      uptime: t * (CONFIG.POLL_INTERVAL / 1000),   // seconds
      heap: Math.round(200 - t * 0.01),             // KB free heap
      rssi: -55 - Math.floor(Math.random() * 15),   // WiFi signal
      timestamp: new Date().toISOString(),
    };
  }

  return { next };
})();

// ── Real API fetch ─────────────────────────────────────────────────────────────
async function fetchSensorData() {
  if (CONFIG.USE_MOCK_DATA) {
    // Simulate async delay
    return new Promise(resolve => setTimeout(() => resolve(MockSensor.next()), 150));
  }

  try {
    const res = await fetch(`${CONFIG.API_BASE_URL}/sensors`, {
      signal: AbortSignal.timeout(5000),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.warn('[FireGuard] API fetch failed:', err.message);
    return null;
  }
}

// ── Thresholds (user-configurable) ─────────────────────────────────────────────
function getThresholds() {
  return {
    tempWarn: Number(localStorage.getItem('fg_tempThreshold') || CONFIG.TEMP_WARN),
    tempDanger: Number(localStorage.getItem('fg_tempThreshold') || CONFIG.TEMP_DANGER),
    smokeWarn: Number(localStorage.getItem('fg_smokeThreshold') || CONFIG.SMOKE_WARN),
    smokeDanger: Number(localStorage.getItem('fg_smokeThreshold') || CONFIG.SMOKE_DANGER),
  };
}

function saveThresholdConfig(temp, smoke) {
  localStorage.setItem('fg_tempThreshold', temp);
  localStorage.setItem('fg_smokeThreshold', smoke);
}
