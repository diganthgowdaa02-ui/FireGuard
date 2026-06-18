/**
 * Vercel Serverless Function — /api/status
 * Proxies to ESP32 /api/status or returns mock device info.
 */

export default async function handler(req, res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Cache-Control', 'no-store');

  const esp32Url = process.env.ESP32_URL;
  const statusUrl = esp32Url ? esp32Url.replace('/sensors', '/status') : null;

  if (statusUrl) {
    try {
      const { default: fetch } = await import('node-fetch');
      const upstream = await fetch(statusUrl, { 
        signal: AbortSignal.timeout(5000),
        headers: {
          'ngrok-skip-browser-warning': 'true',
          'User-Agent': 'FireGuard-Server/1.0',
        },
      });
      const data = await upstream.json();
      return res.status(200).json(data);
    } catch (err) {
      return res.status(502).json({ error: 'ESP32 unreachable', message: err.message });
    }
  }

  return res.status(200).json({
    device:   'FireGuard-ESP32',
    firmware: 'v2.1.4',
    ip:       process.env.ESP32_IP || '(not configured)',
    rssi:     -58,
    uptime:   Math.floor(Date.now() / 1000) % 86400,
    heap:     210,
    online:   !!esp32Url,
    mock:     true,
  });
}
