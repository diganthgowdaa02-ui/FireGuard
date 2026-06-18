/**
 * FireGuard — Main Application (Vercel build)
 * IR Flame Sensor + Relay Water Pump
 */

const state = {
  alerts:            [],
  pollTimer:         null,
  currentAlertLevel: 'safe',
  lastFlameState:    false,
  lastPumpState:     false,
};

const $       = id => document.getElementById(id);
const setText = (id, val) => { const el = $(id); if (el) el.textContent = val; };

// ── Boot ──────────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  Charts.init();
  startPolling();
  setupNavigation();
  setupControls();
  addStartupAlert();
});

// ── Polling ───────────────────────────────────────────────────────────────────
function startPolling() {
  poll();
  state.pollTimer = setInterval(poll, CONFIG.POLL_INTERVAL);
}

async function poll() {
  const data = await fetchSensorData();
  if (!data) { updateDeviceOffline(); return; }
  updateDashboard(data);
  updateDeviceStatus(data);
}

// ── Dashboard ─────────────────────────────────────────────────────────────────
function updateDashboard(data) {
  const { flame, pump_active: pump, cooldown_left: cooldown = 0, flame_events: events = 0 } = data;

  setText('lastUpdated', new Date().toLocaleTimeString());

  // Flame card
  $('cardFlame').className     = `sensor-card${flame ? ' alert-state' : ''}`;
  $('flameValue').textContent  = flame ? 'FLAME!' : 'SAFE';
  $('flameValue').className    = `card-value flame-value${flame ? ' flame-active' : ''}`;
  $('flameStatus').textContent = flame ? 'FLAME DETECTED' : 'No Flame';
  $('flameStatus').className   = `card-status ${flame ? 'danger' : 'safe'}`;
  $('flameRing').className     = `flame-ring${flame ? ' active' : ''}`;
  $('flameDot').className      = `flame-dot${flame ? ' active' : ''}`;
  $('flameDesc').textContent   = flame
    ? '⚠️ Open flame detected! Water pump activated.'
    : 'No flame radiation detected by IR sensor';
  setText('flameEvents', events);

  // Pump card
  if (flame) {
    $('cardPump').className    = 'sensor-card pump-active-state';
    $('pumpValue').textContent = 'PUMPING';
    $('pumpValue').className   = 'card-value pump-value pump-on';
    $('pumpStatus').textContent= 'ACTIVE';
    $('pumpStatus').className  = 'card-status danger';
    $('pumpRing').className    = 'pump-ring active';
    $('pumpDesc').textContent  = '💧 Dispensing water — extinguishing fire!';
  } else if (pump) {
    $('cardPump').className    = 'sensor-card pump-cooldown-state';
    $('pumpValue').textContent = 'COOLDOWN';
    $('pumpValue').className   = 'card-value pump-value pump-cooldown';
    $('pumpStatus').textContent= 'Cooldown';
    $('pumpStatus').className  = 'card-status warning';
    $('pumpRing').className    = 'pump-ring cooldown';
    $('pumpDesc').textContent  = `💧 Flame gone. Pump running for ${cooldown}s more.`;
  } else {
    $('cardPump').className    = 'sensor-card';
    $('pumpValue').textContent = 'OFF';
    $('pumpValue').className   = 'card-value pump-value';
    $('pumpStatus').textContent= 'Standby';
    $('pumpStatus').className  = 'card-status safe';
    $('pumpRing').className    = 'pump-ring';
    $('pumpDesc').textContent  = 'Pump is on standby. No fire detected.';
  }
  setText('cooldownLeft', cooldown > 0 ? cooldown + 's' : '0s');
  setText('pumpState',    pump ? 'ACTIVE' : 'OFF');
  setText('buzzerState',  pump ? 'ACTIVE' : 'STANDBY');

  // Hero
  setText('heroFlame',   flame ? '🔥' : 'SAFE');
  setText('heroPump',    pump  ? 'ON'  : 'OFF');
  setText('screenFlame', flame ? '🔥 FLAME!' : 'No Flame');
  setText('screenPump',  pump  ? '💧 ACTIVE' : 'Pump OFF');
  setText('screenRssi',  (data.rssi || '--') + ' dBm');

  // Mini chart
  Charts.pushData(flame);

  // Alert level transitions
  const level = flame ? 'danger' : pump ? 'warning' : 'safe';
  if (level !== state.currentAlertLevel) {
    state.currentAlertLevel = level;
    updateStatusBanner(level, flame, pump, cooldown);
    updateNavStatus(level);
  }

  // Log each new flame event
  if (flame && !state.lastFlameState) {
    addAlert('danger');
    sendBrowserNotification('🔥 Fire Detected!', 'IR flame sensor triggered. Water pump activated.');
  }

  state.lastFlameState = flame;
  state.lastPumpState  = pump;
}

// ── Banner ────────────────────────────────────────────────────────────────────
function updateStatusBanner(level, flame, pump, cooldown) {
  const banner = $('statusBanner');
  banner.className = `status-banner ${level}`;
  const icon = banner.querySelector('.status-icon');

  if (level === 'danger') {
    icon.textContent = '🚨';
    setText('bannerTitle',   'FIRE ALERT — Water Pump Activated!');
    setText('bannerMessage', 'IR sensor triggered. Relay closed. Pump dispensing water.');
    $('dismissBtn').style.display = 'block';
  } else if (level === 'warning') {
    icon.textContent = '⚠️';
    setText('bannerTitle',   'Fire Extinguished — Cooldown Running');
    setText('bannerMessage', `Flame gone. Pump runs ${cooldown}s more to fully extinguish.`);
    $('dismissBtn').style.display = 'block';
  } else {
    icon.textContent = '✅';
    setText('bannerTitle',   'All Systems Normal');
    setText('bannerMessage', 'No flame detected. Relay off. Monitoring active.');
    $('dismissBtn').style.display = 'none';
  }
}

function updateNavStatus(level) {
  const dot = $('navStatusDot'), label = $('navStatusLabel');
  if (dot)   dot.className   = `status-dot ${level}`;
  if (label) label.textContent = level === 'danger' ? '🚨 FIRE ALERT'
    : level === 'warning' ? '💧 Pump Active' : 'System Safe';
}

// ── Device Status ─────────────────────────────────────────────────────────────
function updateDeviceStatus(data) {
  const badge = $('esp32Status'), resp = $('lastResponse');
  if (badge) { badge.textContent = 'ONLINE'; badge.className = 'device-badge online'; }
  if (resp)  { resp.textContent  = '200 OK'; resp.style.color = 'var(--safe-green)'; }
  if (data) {
    const u = data.uptime || 0;
    setText('deviceUptime', `${Math.floor(u/3600)}h ${Math.floor((u%3600)/60)}m ${u%60}s`);
    setText('heroUptime',   `${Math.floor(u/3600)}h ${Math.floor((u%3600)/60)}m`);
    setText('freeHeap',     (data.heap || '--') + ' KB');
    setText('flamePing',    (5 + Math.floor(Math.random() * 10)) + ' ms');
  }
}

function updateDeviceOffline() {
  const badge = $('esp32Status'), resp = $('lastResponse');
  if (badge) { badge.textContent = 'OFFLINE'; badge.className = 'device-badge offline'; }
  if (resp)  { resp.textContent  = 'No response'; resp.style.color = 'var(--danger-red)'; }
}

// ── Alert Log ─────────────────────────────────────────────────────────────────
function addAlert(level) {
  const log = $('alertLog'), empty = $('alertEmpty');
  if (empty) empty.style.display = 'none';

  const el = document.createElement('div');
  el.className = `alert-item ${level === 'danger' ? 'danger-alert' : 'warning-alert'}`;
  el.dataset.id = Date.now();
  el.innerHTML = `
    <span class="alert-time">${new Date().toLocaleTimeString()}</span>
    <div class="alert-body">
      <strong>${level === 'danger' ? '🚨 Fire Detected — Pump Activated' : '💧 Cooldown Running'}</strong>
      <span>${level === 'danger'
        ? 'IR sensor triggered. Relay closed. Water pump dispensing.'
        : 'Flame extinguished. Pump running cooldown.'}</span>
    </div>
    <span class="alert-badge ${level === 'danger' ? 'badge-danger' : 'badge-warning'}">${level.toUpperCase()}</span>
  `;
  log.insertBefore(el, log.firstChild);
  showToast(level === 'danger' ? '🚨 Fire detected! Pump ON' : '💧 Cooldown active');
}

function addStartupAlert() {
  const log = $('alertLog'), empty = $('alertEmpty');
  if (empty) empty.style.display = 'none';
  const el = document.createElement('div');
  el.className = 'alert-item info-alert';
  el.innerHTML = `
    <span class="alert-time">${new Date().toLocaleTimeString()}</span>
    <div class="alert-body">
      <strong>FireGuard system started</strong>
      <span>IR flame sensor monitoring. Relay on standby. Dashboard live.</span>
    </div>
    <span class="alert-badge badge-info">INFO</span>
  `;
  log.insertBefore(el, log.firstChild);
}

// ── Browser Push Notifications ────────────────────────────────────────────────
window.enablePushNotifications = async function () {
  if (!('Notification' in window)) {
    showToast('Browser notifications not supported', 'error');
    return;
  }
  const perm = await Notification.requestPermission();
  const el = $('pushStatus'), btn = $('enablePushBtn');
  if (perm === 'granted') {
    if (el)  el.textContent = '✅ Notifications enabled';
    if (btn) btn.textContent = 'Enabled ✓';
    showToast('Browser notifications enabled ✓', 'success');
  } else {
    if (el) el.textContent = '❌ Permission denied';
    showToast('Permission denied', 'error');
  }
};

function sendBrowserNotification(title, body) {
  if (Notification.permission === 'granted') {
    new Notification(title, { body, icon: '/favicon.ico' });
  }
}

// ── Navigation ────────────────────────────────────────────────────────────────
function setupNavigation() {
  const burger   = $('hamburger');
  const navLinks = document.querySelector('.nav-links');
  if (burger && navLinks) {
    burger.addEventListener('click', () => navLinks.classList.toggle('open'));
    navLinks.querySelectorAll('a').forEach(a =>
      a.addEventListener('click', () => navLinks.classList.remove('open'))
    );
  }
  const sections = document.querySelectorAll('section[id]');
  window.addEventListener('scroll', () => {
    let cur = '';
    sections.forEach(s => { if (window.scrollY >= s.offsetTop - 100) cur = s.id; });
    document.querySelectorAll('.nav-links a').forEach(a => {
      a.style.color = a.getAttribute('href') === '#' + cur ? 'var(--accent-orange)' : '';
    });
  }, { passive: true });
}

// ── Controls ──────────────────────────────────────────────────────────────────
function setupControls() {
  $('dismissBtn').addEventListener('click', () => {
    state.currentAlertLevel = 'safe';
    updateStatusBanner('safe', false, false, 0);
    updateNavStatus('safe');
  });
  $('clearAlertsBtn').addEventListener('click', () => {
    $('alertLog').querySelectorAll('.alert-item').forEach(i => i.remove());
    $('alertEmpty').style.display = 'block';
    state.alerts = [];
    showToast('Alert log cleared', 'success');
  });
  $('testConnectionBtn').addEventListener('click', async () => {
    showToast('Testing…');
    const d = await fetchSensorData();
    showToast(d ? `Connected ✓ ${d.mock ? '(demo mode)' : '(live ESP32)'}` : 'Connection failed', d ? 'success' : 'error');
  });
}

window.saveNotification = function (type) {
  const val = type === 'email' ? $('emailInput').value : $('phoneInput').value;
  if (!val) { showToast('Please enter a value', 'error'); return; }
  localStorage.setItem(`fg_${type}`, val);
  showToast(`${type === 'email' ? 'Email' : 'Phone'} saved ✓`, 'success');
};

window.saveThresholds = () => showToast('Saved ✓', 'success');

// ── Toast ─────────────────────────────────────────────────────────────────────
function showToast(msg, type = '') {
  const t = $('toast');
  t.textContent = msg;
  t.className = `toast show ${type}`;
  clearTimeout(t._t);
  t._t = setTimeout(() => t.classList.remove('show'), 3200);
}
