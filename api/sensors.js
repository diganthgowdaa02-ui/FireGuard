/**
 * Vercel Serverless — /api/sensors
 * Reads live sensor data from Firebase Realtime Database.
 * No ESP32 IP needed. Works from anywhere.
 *
 * Set in Vercel Environment Variables:
 *   FIREBASE_URL = https://fireguard-xxxxx-default-rtdb.firebaseio.com
 */

export default async function handler(req, res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  res.setHeader('Cache-Control', 'no-store');

  if (req.method === 'OPTIONS') return res.status(200).end();

  const firebaseUrl = process.env.FIREBASE_URL;

  // ── Read from Firebase ─────────────────────────────────────────────────────
  if (firebaseUrl) {
    try {
      const { default: fetch } = await import('node-fetch');

      // Firebase REST API — just append .json to the path
      const url = `${firebaseUrl}/fireguard/sensors.json`;
      const response = await fetch(url, {
        signal: AbortSignal.timeout(5000),
      });

      if (!response.ok) throw new Error(`Firebase responded with ${response.status}`);

      const data = await response.json();

      if (!data) {
        // Firebase returned null — ESP32 hasn't pushed data yet
        return res.status(200).json({
          flame: false, pump_active: false, cooldown_left: 0,
          flame_events: 0, alert_level: 0, uptime: 0,
          heap: 0, rssi: 0, timestamp: Date.now(),
          mock: true, message: 'Waiting for ESP32 to push data...',
        });
      }

      // Real data from ESP32 — no mock flag
      return res.status(200).json({ ...data, mock: false });

    } catch (err) {
      console.error('[FireGuard] Firebase read failed:', err.message);
      return res.status(502).json({
        error: 'Firebase unreachable',
        message: err.message,
      });
    }
  }

  // ── Mock fallback (FIREBASE_URL not set) ───────────────────────────────────
  const uptime  = Math.floor(Date.now() / 1000) % 86400;
  const cycle   = uptime % 90;
  const flame   = cycle >= 80 && cycle <= 89;
  const pump    = cycle >= 80 && cycle <= 99;

  return res.status(200).json({
    flame,
    pump_active:   pump,
    cooldown_left: pump && !flame ? Math.max(0, 99 - cycle) : 0,
    flame_events:  Math.floor(uptime / 90),
    alert_level:   flame ? 2 : pump ? 1 : 0,
    uptime, heap: 210, rssi: -58,
    timestamp: Date.now(),
    mock: true,
  });
}
