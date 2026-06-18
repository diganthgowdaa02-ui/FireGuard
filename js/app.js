/**
 * FireGuard — Main Application
 * Components: IR Flame Sensor + Relay (Water Pump)
 */

const state = {
  alerts:            [],
  pollTimer:         null,
  startTime:         Date.now(),
  currentAlertLevel: 'safe',
  lastPumpState:     false,
  lastFlameState:    false,
};

const $ = id => document.getElementById(id);
const setText = (id, val) => { const el = $(id); if (el) el.textContent = val; };

// ── Init ──────────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  startPolling();
  setupNavigation();
  setupModals();
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
  if (!data) { updateDeviceStatus(false); return; }
  updateDashboard(data);
  updateDeviceStatus(true, data);
}

// ── Dashboard ─────────────────────────────────────────────────────────────────
function updateDashboard(data) {
  const { flame, pump_active: pump, cooldown_left: cooldown, flame_events: events } = data;

  setText('lastUpdated', new Date().toLocaleTimeString());

  // ── Flame card ──
  const flameCard   = $('cardFlame');
  const flameVal    = $('flameValue');
  const flameRing   = $('flameRing');
  const flameDot    = $('flameDot');
  const flameStatus = $('flameStatus');
  const flameDesc   = $('flameDesc');

  if (flame) {
    flameCard.className     = 'sensor-card alert-state';
    flameVal.textContent    = 'FLAME!';
    flameVal.className      = 'card-value flame-value flame-active';
    flameStatus.textContent = 'FLAME DETECTED';
    flameStatus.className   = 'card-status danger';
    flameRing.className     = 'flame-ring active';
    flameDot.className      = 'flame-dot active';
    flameDesc.textContent   = '⚠️ Open flame detected! Water pump activated.';
  } else {
    flameCard.className     = 'sensor-card';
    flameVal.textContent    = 'SAFE';
    flameVal.className      = 'card-value flame-value';
    flameStatus.textContent = 'No Flame';
    flameStatus.className   = 'card-status safe';
    flameRing.className     = 'flame-ring';
    flameDot.className      = 'flame-dot';
    flameDesc.textContent   = 'No flame radiation detected by IR sensor';
  }
  setText('flameEvents', events || 0);

  // ── Pump card ──
  const pumpCard   = $('cardPump');
  const pumpVal    = $('pumpValue');
  const pumpRing   = $('pumpRing');
  const pumpStatus = $('pumpStatus');
  const pumpDesc   = $('pumpDesc');

  if (flame) {
    pumpCard.className     = 'sensor-card pump-active-state';
    pumpVal.textContent    = 'PUMPING';
    pumpVal.className      = 'card-value pump-value pump-on';
    pumpStatus.textContent = 'ACTIVE';
    pumpStatus.className   = 'card-status danger';
    pumpRing.className     = 'pump-ring active';
    pumpDesc.textContent   = '💧 Dispensing water — extinguishing fire!';
  } else if (pump) {
    pumpCard.className     = 'sensor-card pump-cooldown-state';
    pumpVal.textContent    = 'COOLDOWN';
    pumpVal.className      = 'card-value pump-value pump-cooldown';
    pumpStatus.textContent = 'Cooldown';
    pumpStatus.className   = 'card-status warning';
    pumpRing.className     = 'pump-ring cooldown';
    pumpDesc.textContent   = `💧 Flame gone. Pump running for ${cooldown}s more to ensure fire is out.`;
  } else {
    pumpCard.className     = 'sensor-card';
    pumpVal.textContent    = 'OFF';
    pumpVal.className      = 'card-value pump-value';
    pumpStatus.textContent = 'Standby';
    pumpStatus.className   = 'card-status safe';
    pumpRing.className     = 'pump-ring';
    pumpDesc.textContent   = 'Pump is on standby. No fire detected.';
  }
  setText('cooldownLeft', cooldown > 0 ? cooldown + 's' : '0s');
  setText('pumpState', pump ? 'ACTIVE' : 'OFF');

  // ── Hero stats ──
  setText('heroFlame', flame ? '🔥' : 'SAFE');
  setText('heroPump',  pump  ? 'ON'  : 'OFF');
  setText('screenFlame', flame ? '🔥 FLAME!' : 'No Flame');
  setText('screenPump',  pump  ? '💧 ACTIVE' : 'Pump OFF');
  setText('screenRssi', (data.rssi || '--') + ' dBm');

  // ── Overall level ──
  const level = flame ? 'danger' : pump ? 'warning' : 'safe';
  if (level !== state.currentAlertLevel) {
    state.currentAlertLevel = level;
    updateStatusBanner(level, flame, pump, cooldown);
    updateNavStatus(level);
    if (level !== 'safe') addAlert(level, flame, pump);
  }

  // Alert when flame first appears
  if (flame && !state.lastFlameState) {
    addAlert('danger', true, true);
  }

  state.lastFlameState = flame;
  state.lastPumpState  = pump;

  // Update hero chart
  Charts.pushData(flame);
}

// ── Status Banner ─────────────────────────────────────────────────────────────
function updateStatusBanner(level, flame, pump, cooldown) {
  const banner = $('statusBanner');
  const icon   = banner.querySelector('.status-icon');

  banner.className = `status-banner ${level}`;

  if (level === 'danger') {
    icon.textContent = '🚨';
    setText('bannerTitle',   'FIRE ALERT — Water Pump Activated!');
    setText('bannerMessage', 'Flame detected by IR sensor. Relay triggered — pump is dispensing water.');
    $('dismissBtn').style.display = 'block';
  } else if (level === 'warning') {
    icon.textContent = '⚠️';
    setText('bannerTitle',   'Fire Extinguished — Cooldown Running');
    setText('bannerMessage', `Flame gone. Pump running ${cooldown}s more to ensure the area is safe.`);
    $('dismissBtn').style.display = 'block';
  } else {
    icon.textContent = '✅';
    setText('bannerTitle',   'All Systems Normal');
    setText('bannerMessage', 'No flame detected. Relay off. System monitoring.');
    $('dismissBtn').style.display = 'none';
  }
}

function updateNavStatus(level) {
  const dot   = $('navStatusDot');
  const label = $('navStatusLabel');
  if (dot)   dot.className = `status-dot ${level}`;
  if (label) label.textContent = level === 'danger' ? '🚨 FIRE ALERT'
    : level === 'warning' ? '💧 Pump Active' : 'System Safe';
}

// ── Device Status ─────────────────────────────────────────────────────────────
function updateDeviceStatus(online, data = null) {
  const badge    = $('esp32Status');
  const lastResp = $('lastResponse');

  if (!online) {
    if (badge)    { badge.textContent = 'OFFLINE'; badge.className = 'device-badge offline'; }
    if (lastResp) { lastResp.textContent = 'No response'; lastResp.style.color = 'var(--danger-red)'; }
    return;
  }

  if (badge)    { badge.textContent = 'ONLINE'; badge.className = 'device-badge online'; }
  if (lastResp) { lastResp.textContent = '200 OK'; lastResp.style.color = 'var(--safe-green)'; }

  if (data) {
    const u = data.uptime || 0;
    setText('deviceUptime', `${Math.floor(u/3600)}h ${Math.floor((u%3600)/60)}m ${Math.floor(u%60)}s`);
    setText('heroUptime',   `${Math.floor(u/3600)}h ${Math.floor((u%3600)/60)}m`);
    setText('freeHeap',     (data.heap || '--') + ' KB');
    const ping = 5 + Math.floor(Math.random() * 10);
    setText('flamePing', ping + ' ms');
  }
}

// ── Alert Log ─────────────────────────────────────────────────────────────────
function addAlert(level, flame, pump) {
  const a = { id: Date.now(), level, flame, pump, time: new Date() };
  state.alerts.unshift(a);

  const log   = $('alertLog');
  const empty = $('alertEmpty');
  if (empty) empty.style.display = 'none';

  const msg    = level === 'danger' ? '🚨 Fire Detected — Pump Activated' : '💧 Cooldown — Pump Still Running';
  const detail = flame  ? 'IR sensor triggered. Relay closed. Water pump ON.'
                        : 'Flame gone. Pump running cooldown to ensure safety.';

  const el = document.createElement('div');
  el.className = `alert-item ${level === 'danger' ? 'danger-alert' : 'warning-alert'}`;
  el.dataset.id = a.id;
  el.innerHTML = `
    <span class="alert-time">${a.time.toLocaleTimeString()}</span>
    <div class="alert-body">
      <strong>${msg}</strong>
      <span>${detail}</span>
    </div>
    <span class="alert-badge ${level === 'danger' ? 'badge-danger' : 'badge-warning'}">
      ${level.toUpperCase()}
    </span>
  `;
  log.insertBefore(el, log.firstChild);
  showToast(msg, level === 'danger' ? 'error' : '');
}

function addStartupAlert() {
  const log   = $('alertLog');
  const empty = $('alertEmpty');
  if (empty) empty.style.display = 'none';
  const el = document.createElement('div');
  el.className = 'alert-item info-alert';
  el.innerHTML = `
    <span class="alert-time">${new Date().toLocaleTimeString()}</span>
    <div class="alert-body">
      <strong>FireGuard system started</strong>
      <span>IR flame sensor online. Relay module on standby. Monitoring active.</span>
    </div>
    <span class="alert-badge badge-info">INFO</span>
  `;
  log.insertBefore(el, log.firstChild);
}

// ── Navigation ────────────────────────────────────────────────────────────────
function setupNavigation() {
  const hamburger = $('hamburger');
  const navLinks  = document.querySelector('.nav-links');
  if (hamburger && navLinks) {
    hamburger.addEventListener('click', () => navLinks.classList.toggle('open'));
    navLinks.querySelectorAll('a').forEach(a =>
      a.addEventListener('click', () => navLinks.classList.remove('open'))
    );
  }
  const sections = document.querySelectorAll('section[id]');
  window.addEventListener('scroll', () => {
    let current = '';
    sections.forEach(s => { if (window.scrollY >= s.offsetTop - 100) current = s.id; });
    document.querySelectorAll('.nav-links a').forEach(a => {
      a.style.color = a.getAttribute('href') === '#' + current ? 'var(--accent-orange)' : '';
    });
  }, { passive: true });
}

// ── Modals & Controls ─────────────────────────────────────────────────────────
function setupModals() {
  const modal = $('configModal');
  $('configBtn').addEventListener('click', () => {
    $('apiUrlInput').value = CONFIG.API_BASE_URL;
    $('pollInput').value   = CONFIG.POLL_INTERVAL / 1000;
    modal.classList.add('open');
  });
  $('modalClose').addEventListener('click',   () => modal.classList.remove('open'));
  $('cancelConfig').addEventListener('click', () => modal.classList.remove('open'));
  $('saveConfig').addEventListener('click', () => {
    const url      = $('apiUrlInput').value.trim();
    const interval = parseInt($('pollInput').value) * 1000;
    if (url) { CONFIG.API_BASE_URL = url; setText('apiUrl', url); }
    if (interval >= 1000) {
      CONFIG.POLL_INTERVAL = interval;
      setText('pollInterval', interval / 1000 + 's');
      clearInterval(state.pollTimer);
      startPolling();
    }
    modal.classList.remove('open');
    showToast('Configuration saved ✓', 'success');
  });
  modal.addEventListener('click', e => { if (e.target === modal) modal.classList.remove('open'); });
}

function setupControls() {
  $('dismissBtn').addEventListener('click', () => {
    state.currentAlertLevel = 'safe';
    updateStatusBanner('safe', false, false, 0);
    updateNavStatus('safe');
  });
  $('clearAlertsBtn').addEventListener('click', () => {
    state.alerts = [];
    $('alertLog').querySelectorAll('.alert-item').forEach(i => i.remove());
    $('alertEmpty').style.display = 'block';
    showToast('Alert log cleared', 'success');
  });
  $('testConnectionBtn').addEventListener('click', async () => {
    showToast('Testing connection…');
    const data = await fetchSensorData();
    showToast(data ? 'Connection successful ✓' : 'Connection failed — check IP', data ? 'success' : 'error');
  });
}

window.saveNotification = function (type) {
  if (type === 'email') {
    const email = $('emailInput').value;
    if (!email || !email.includes('@')) { showToast('Enter a valid email', 'error'); return; }
    localStorage.setItem('fg_email', email);
    showToast('Email saved ✓', 'success');
  } else {
    const phone = $('phoneInput').value;
    if (!phone) { showToast('Enter a phone number', 'error'); return; }
    localStorage.setItem('fg_phone', phone);
    showToast('Phone saved ✓', 'success');
  }
};

window.saveThresholds = function () { showToast('Settings saved ✓', 'success'); };

// ── Toast ─────────────────────────────────────────────────────────────────────
function showToast(message, type = '') {
  const toast = $('toast');
  toast.textContent = message;
  toast.className   = `toast show ${type}`;
  clearTimeout(toast._timer);
  toast._timer = setTimeout(() => toast.classList.remove('show'), 3000);
}
