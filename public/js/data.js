/**
 * FireGuard — Data Layer
 *
 * Reads directly from Firebase Realtime Database REST API.
 * No Vercel proxy needed — browser fetches Firebase directly.
 * This works from anywhere as long as Firebase rules allow .read = true
 */

const CONFIG = {
  // Your Firebase Realtime Database URL
  FIREBASE_URL:  'https://fireguard-dfb77-default-rtdb.firebaseio.com',
  POLL_INTERVAL: 2000,
};

async function fetchSensorData() {
  try {
    // Firebase REST API: append .json to any path to read it
    const url = `${CONFIG.FIREBASE_URL}/fireguard/sensors.json`;

    const res = await fetch(url, {
      signal: AbortSignal.timeout(6000),
      cache: 'no-store',
    });

    if (!res.ok) throw new Error(`Firebase HTTP ${res.status}`);

    const data = await res.json();

    // Firebase returns null if path doesn't exist yet
    if (!data) {
      console.warn('[FireGuard] Firebase path empty — ESP32 not pushed yet');
      return null;
    }

    console.log('[FireGuard] Got data:', data);
    return data;

  } catch (err) {
    console.warn('[FireGuard] Firebase fetch error:', err.message);
    return null;
  }
}
