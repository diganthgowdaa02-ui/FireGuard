/**
 * Vercel Serverless Function — /api/sensors
 *
 * Proxies the request to your ESP32 device.
 * Set the environment variable ESP32_URL in your Vercel project settings:
 *   ESP32_URL = http://<your-esp32-ip>/api/sensors
 *
 * If ESP32_URL is not set, returns realistic mock data so the
 * dashboard works even before the hardware is connected.
 */

export default async function handler(req, res) {
  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  res.setHeader('Cache-Control', 'no-store');

  if (req.method === 'OPTIONS') {
    return res.status(200).end();
  }

  const esp32Url = process.env.ESP32_URL;

  // ── Forward to real ESP32 ──────────────────────────────────────────────────
  if (esp32Url) {
    try {
      const { default: fetch } = await import('node-fetch');
      const upstream = await fetch(esp32Url, {
        signal: AbortSignal.timeout(5000),
      });

      if (!upstream.ok) {
        throw new Error(`ESP32 responded with ${upstream.status}`);
      }

      const data = await upstream.json();
      return res.status(200).json(data);

    } catch (err) {
      console.error('[FireGuard] ESP32 fetch failed:', err.message);
      return res.status(502).json({
        error: 'ESP32 unreachable',
        message: err.message,
        hint: 'Check that ESP32_URL env variable is correct and device is online.',
      });
    }
  }

  // ── Mock fallback (no ESP32_URL set) ──────────────────────────────────────
  // Returns plausible demo data so the Vercel dashboard is always live.
  const now       = Date.now();
  const uptime    = Math.floor(now / 1000) % 86400;

  // Simulate a brief fire event every ~90 seconds for demo purposes
  const cycle     = uptime % 90;
  const flame     = cycle >= 80 && cycle <= 89;
  const pumpActive = cycle >= 80 && cycle <= 89 + 10;
  const cooldownLeft = pumpActive && !flame ? Math.max(0, 90 + 10 - cycle) : 0;

  return res.status(200).json({
    flame,
    pump_active:    pumpActive,
    cooldown_left:  cooldownLeft,
    flame_events:   Math.floor(uptime / 90),
    alert_level:    flame ? 2 : pumpActive ? 1 : 0,
    uptime,
    heap:           210,
    rssi:           -58,
    last_flame_sec: flame ? 0 : (uptime % 90),
    timestamp:      now,
    mock:           true,    // lets the UI show a "demo mode" badge
  });
}
