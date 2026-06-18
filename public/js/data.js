/**
 * FireGuard — Data Layer (Vercel version)
 * Hits the Vercel serverless proxy at /api/sensors
 * which forwards to your ESP32 via the ESP32_URL env variable.
 */

const CONFIG = {
  API_BASE_URL:  '/api',      // Vercel serverless route — no hardcoded IP needed
  POLL_INTERVAL: 2000,
};

async function fetchSensorData() {
  try {
    const res = await fetch(`${CONFIG.API_BASE_URL}/sensors`, {
      signal: AbortSignal.timeout(6000),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();

    // Hide demo banner if real ESP32 data is coming through
    if (!data.mock) {
      const banner = document.getElementById('demoBanner');
      if (banner) banner.style.display = 'none';
    }

    return data;
  } catch (err) {
    console.warn('[FireGuard] Fetch error:', err.message);
    return null;
  }
}
