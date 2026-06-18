/**
 * FireGuard — Main Application
 * Polls ESP32 sensor API, updates the UI, manages alerts and notifications.
 */

// ── State ──────────────────────────────────────────────────────────────────────
const state = {
  lastData: null,
  alerts: [],
  pollTimer: null,
  startTime: Date.now(),
  currentAlertLevel: 'safe',
};

// ── DOM helpers ────────────────────────────────────────────────────────────────
const $ = id => document.getElementById(id);
const setText = (id, val) => { const el = $(id); if (el) el.textContent = val; };
const setClass = (el, cls) => { if (el) el.className = cls; };

// ── Init ───────────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  Charts.init();
  restoreSettings();
  startPolling();
  setupNavigation();
  setupModals();
  setupControls();
  addStartupAlert();
});

// ── Polling ────────────────────────────────────────────────────────────────────
function startPolling() {
  poll();
  state.pollTimer = setInterval(poll, CONFIG.POLL_INTERVAL);
}

async function poll() {
  const data = await fetchSensorData();
  if (!data) {
    updateDeviceStatus(false);
    return;
  }
  state.lastData = data;
  updateDashboard(data);
  updateDeviceStatus(true, data);
}

// ── Dashboard Update ───────────────────────────────────────────────────────────
function updateDashboard(data) {
  const { temperature: temp, smoke, humidity: hum, flame } = data;
  const th = getThresholds();

  // Timestamp
  setText('lastUpdated', new Date().toLocaleTimeString());

  // ── Temperature ──
  setText('tempValue', temp.toFixed(1));
  $('tempBar').style.width = Math.min((temp / 100) * 100, 100) + '%';
  const tempLevel = temp >= th.tempDanger ? 'danger' : temp >= th.tempWarn ? 'warning' : 'safe';
  updateCardStatus('cardTemp', 'tempStatus', tempLevel,
    { safe: 'Normal', warning: 'High Temp', danger: 'CRITICAL' });

  // ── Smoke ──
  setText('smokeValue', smoke);
  $('smokeBar').style.width = Math.min((smoke / 1000) * 100, 100) + '%';
  const smokeLevel = smoke >= th.smokeDanger ? 'danger' : smoke >= th.smokeWarn ? 'warning' : 'safe';
  updateCardStatus('cardSmoke', 'smokeStatus', smokeLevel,
    { safe: 'Normal', warning: 'Elevated', danger: 'CRITICAL' });

  // ── Flame ──
  const flameEl = $('flameValue');
  const flameDesc = $('flameDesc');
  const flameRing = document.querySelector('.flame-ring');
  const flameDot = document.querySelector('.flame-dot');

  if (flame) {
    flameEl.textContent = 'FLAME!';
    flameEl.className = 'card-value flame-value flame-active';
    setText('flameStatus', 'FLAME DETECTED');
    $('flameStatus').className = 'card-status danger';
    if (flameRing) flameRing.className = 'flame-ring active';
    if (flameDot) flameDot.className = 'flame-dot active';
    if (flameDesc) flameDesc.textContent = '⚠️ Open flame detected by IR sensor!';
  } else {
    flameEl.textContent = 'SAFE';
    flameEl.className = 'card-value flame-value';
    setText('flameStatus', 'No Flame');
    $('flameStatus').className = 'card-status safe';
    if (flameRing) flameRing.className = 'flame-ring';
    if (flameDot) flameDot.className = 'flame-dot';
    if (flameDesc) flameDesc.textContent = 'No flame radiation detected by IR sensor';
  }

  // ── Humidity ──
  setText('humValue', hum.toFixed(1));
  $('humBar').style.width = hum + '%';
  const humLevel = hum < 20 ? 'warning' : 'safe';
  updateCardStatus('cardHumidity', 'humStatus', humLevel,
    { safe: 'Normal', warning: 'Low Humidity', danger: 'Critical' });

  // ── Hero stats ──
  setText('heroTemp', temp.toFixed(1));
  setText('heroSmoke', smoke);
  setText('screenTemp', temp.toFixed(1) + '°C');
  setText('screenSmoke', smoke + ' ppm');
  setText('screenFlame', flame ? '🔥 FLAME' : 'No Flame');

  // ── Charts ──
  Charts.pushData(temp, smoke, hum);

  // ── System level ──
  const overallLevel = flame || tempLevel === 'danger' || smokeLevel === 'danger' ? 'danger'
    : tempLevel === 'warning' || smokeLevel === 'warning' ? 'warning' : 'safe';

  if (overallLevel !== state.currentAlertLevel) {
    state.currentAlertLevel = overallLevel;
    updateStatusBanner(overallLevel, temp, smoke, flame);
    updateNavStatus(overallLevel);

    if (overallLevel !== 'safe') {
      addAlert(overallLevel, temp, smoke, flame);
    }
  }
}

function updateCardStatus(cardId, statusId, level, labels) {
  const card = $(cardId);
  const status = $(statusId);
  if (card) {
    card.className = 'sensor-card' + (level === 'danger' ? ' alert-state' : '');
  }
  if (status) {
    status.textContent = labels[level];
    status.className = `card-status ${level}`;
  }
}

// ── Status Banner ──────────────────────────────────────────────────────────────
function updateStatusBanner(level, temp, smoke, flame) {
  const banner = $('statusBanner');
  const icon = banner.querySelector('.status-icon');
  const title = $('bannerTitle');
  const msg = $('bannerMessage');
  const dismissBtn = $('dismissBtn');

  banner.className = `status-banner ${level}`;

  if (level === 'danger') {
    icon.textContent = '🚨';
    title.textContent = 'FIRE ALERT — Immediate Action Required!';
    const reasons = [];
    if (flame) reasons.push('open flame detected');
    if (temp >= getThresholds().tempDanger) reasons.push(`temperature at ${temp}°C`);
    if (smoke >= getThresholds().smokeDanger) reasons.push(`smoke at ${smoke}ppm`);
    msg.textContent = `Warning: ${reasons.join(', ')}.`;
    dismissBtn.style.display = 'block';
  } else if (level === 'warning') {
    icon.textContent = '⚠️';
    title.textContent = 'Elevated Readings Detected';
    msg.textContent = `Monitoring: Temp ${temp}°C, Smoke ${smoke}ppm. No immediate threat.`;
    dismissBtn.style.display = 'block';
  } else {
    icon.textContent = '✅';
    title.textContent = 'All Systems Normal';
    msg.textContent = 'No fire or smoke detected. All sensors operating normally.';
    dismissBtn.style.display = 'none';
  }
}

function updateNavStatus(level) {
  const dot = $('navStatusDot');
  const label = $('navStatusLabel');
  if (dot) { dot.className = `status-dot ${level}`; }
  if (label) {
    label.textContent = level === 'danger' ? '🚨 FIRE ALERT'
      : level === 'warning' ? '⚠️ Warning' : 'System Safe';
  }
}

// ── Device Status ──────────────────────────────────────────────────────────────
function updateDeviceStatus(online, data = null) {
  const badge = $('esp32Status');
  const lastResp = $('lastResponse');

  if (!online) {
    if (badge) { badge.textContent = 'OFFLINE'; badge.className = 'device-badge offline'; }
    if (lastResp) { lastResp.textContent = 'No response'; lastResp.style.color = 'var(--danger-red)'; }
    return;
  }

  if (badge) { badge.textContent = 'ONLINE'; badge.className = 'device-badge online'; }
  if (lastResp) { lastResp.textContent = '200 OK'; lastResp.style.color = 'var(--safe-green)'; }

  if (data) {
    const uptime = data.uptime || 0;
    const h = Math.floor(uptime / 3600);
    const m = Math.floor((uptime % 3600) / 60);
    const s = Math.floor(uptime % 60);
    setText('deviceUptime', `${h}h ${m}m ${s}s`);
    setText('heroUptime', `${h}h ${m}m`);
    setText('freeHeap', (data.heap || '--') + ' KB');

    // Ping simulation
    const ping = 5 + Math.floor(Math.random() * 15);
    setText('dht22Ping', ping + ' ms');
    setText('mq2Ping', (ping + 1) + ' ms');
    setText('flamePing', (ping + 2) + ' ms');
  }
}

// ── Alert Log ──────────────────────────────────────────────────────────────────
function addAlert(level, temp, smoke, flame) {
  const alert = {
    id: Date.now(),
    level,
    temp,
    smoke,
    flame,
    time: new Date(),
  };
  state.alerts.unshift(alert);
  renderAlertLog();
  showToast(`${level === 'danger' ? '🚨' : '⚠️'} ${level.toUpperCase()}: Temp ${temp}°C, Smoke ${smoke}ppm`, level);
}

function addStartupAlert() {
  const time = new Date();
  const item = createAlertElement({
    level: 'info',
    message: 'FireGuard system started',
    detail: 'All sensors online. Monitoring active.',
    time,
    badge: 'INFO',
    badgeClass: 'badge-info',
    cssClass: 'info-alert',
  });
  const log = $('alertLog');
  const empty = $('alertEmpty');
  if (empty) empty.style.display = 'none';
  if (log) log.insertBefore(item, log.firstChild);
}

function renderAlertLog() {
  const log = $('alertLog');
  if (!log) return;
  $('alertEmpty').style.display = 'none';

  state.alerts.slice(0, 50).forEach(a => {
    // Avoid duplicate renders
    if (log.querySelector(`[data-id="${a.id}"]`)) return;

    const reasons = [];
    if (a.flame) reasons.push('Flame detected');
    if (a.temp >= getThresholds().tempWarn) reasons.push(`Temp: ${a.temp}°C`);
    if (a.smoke >= getThresholds().smokeWarn) reasons.push(`Smoke: ${a.smoke}ppm`);

    const item = createAlertElement({
      id: a.id,
      level: a.level,
      message: a.level === 'danger' ? '🚨 Fire Alert Triggered' : '⚠️ Warning: Elevated Readings',
      detail: reasons.join(' · '),
      time: a.time,
      badge: a.level.toUpperCase(),
      badgeClass: a.level === 'danger' ? 'badge-danger' : 'badge-warning',
      cssClass: a.level === 'danger' ? 'danger-alert' : 'warning-alert',
    });
    log.insertBefore(item, log.firstChild);
  });
}

function createAlertElement({ id, level, message, detail, time, badge, badgeClass, cssClass }) {
  const el = document.createElement('div');
  el.className = `alert-item ${cssClass}`;
  if (id) el.dataset.id = id;
  el.innerHTML = `
    <span class="alert-time">${time.toLocaleTimeString()}</span>
    <div class="alert-body">
      <strong>${message}</strong>
      <span>${detail}</span>
    </div>
    <span class="alert-badge ${badgeClass}">${badge}</span>
  `;
  return el;
}

// ── Navigation ─────────────────────────────────────────────────────────────────
function setupNavigation() {
  const hamburger = $('hamburger');
  const navLinks = document.querySelector('.nav-links');
  if (hamburger && navLinks) {
    hamburger.addEventListener('click', () => navLinks.classList.toggle('open'));
    navLinks.querySelectorAll('a').forEach(a => {
      a.addEventListener('click', () => navLinks.classList.remove('open'));
    });
  }

  // Active section highlight
  const sections = document.querySelectorAll('section[id]');
  window.addEventListener('scroll', () => {
    let current = '';
    sections.forEach(s => {
      if (window.scrollY >= s.offsetTop - 100) current = s.id;
    });
    document.querySelectorAll('.nav-links a').forEach(a => {
      a.style.color = a.getAttribute('href') === '#' + current
        ? 'var(--accent-orange)' : '';
    });
  }, { passive: true });
}

// ── Modals & Controls ──────────────────────────────────────────────────────────
function setupModals() {
  const modal = $('configModal');
  $('configBtn').addEventListener('click', () => {
    $('apiUrlInput').value = CONFIG.API_BASE_URL;
    $('pollInput').value = CONFIG.POLL_INTERVAL / 1000;
    modal.classList.add('open');
  });
  $('modalClose').addEventListener('click', () => modal.classList.remove('open'));
  $('cancelConfig').addEventListener('click', () => modal.classList.remove('open'));
  $('saveConfig').addEventListener('click', () => {
    const url = $('apiUrlInput').value.trim();
    const interval = parseInt($('pollInput').value) * 1000;
    if (url) {
      CONFIG.API_BASE_URL = url;
      setText('apiUrl', url);
    }
    if (interval >= 1000) {
      CONFIG.POLL_INTERVAL = interval;
      setText('pollInterval', (interval / 1000) + 's');
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
    updateStatusBanner('safe', 0, 0, false);
    updateNavStatus('safe');
  });
  $('clearAlertsBtn').addEventListener('click', () => {
    state.alerts = [];
    const log = $('alertLog');
    const items = log.querySelectorAll('.alert-item:not(#alertEmpty)');
    items.forEach(i => i.remove());
    $('alertEmpty').style.display = 'block';
    showToast('Alert log cleared', 'success');
  });
  $('testConnectionBtn').addEventListener('click', async () => {
    showToast('Testing connection…');
    const data = await fetchSensorData();
    if (data) showToast('Connection successful ✓', 'success');
    else showToast('Connection failed — check IP', 'error');
  });
}

// ── Settings ───────────────────────────────────────────────────────────────────
function restoreSettings() {
  const tempT = localStorage.getItem('fg_tempThreshold');
  const smokeT = localStorage.getItem('fg_smokeThreshold');
  if (tempT && $('tempThreshold')) $('tempThreshold').value = tempT;
  if (smokeT && $('smokeThreshold')) $('smokeThreshold').value = smokeT;
}

window.saveThresholds = function () {
  const temp = $('tempThreshold').value;
  const smoke = $('smokeThreshold').value;
  saveThresholdConfig(temp, smoke);
  showToast('Thresholds saved ✓', 'success');
};

window.saveNotification = function (type) {
  if (type === 'email') {
    const email = $('emailInput').value;
    if (!email || !email.includes('@')) { showToast('Enter a valid email address', 'error'); return; }
    localStorage.setItem('fg_email', email);
    showToast('Email saved ✓', 'success');
  } else {
    const phone = $('phoneInput').value;
    if (!phone) { showToast('Enter a phone number', 'error'); return; }
    localStorage.setItem('fg_phone', phone);
    showToast('Phone number saved ✓', 'success');
  }
};

// ── Toast ──────────────────────────────────────────────────────────────────────
function showToast(message, type = '') {
  const toast = $('toast');
  toast.textContent = message;
  toast.className = `toast show ${type}`;
  clearTimeout(toast._timer);
  toast._timer = setTimeout(() => toast.classList.remove('show'), 3000);
}
