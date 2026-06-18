/**
 * FireGuard — Dashboard App
 * Clean, values-first UI
 */

const state = {
  prevFlame:    false,
  prevPump:     false,
  alertLevel:   'connecting',
  logItems:     [],
};

const $   = id => document.getElementById(id);
const set = (id, v) => { const e = $(id); if (e) e.textContent = v; };

// ── Boot ──────────────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  poll();
  setInterval(poll, CONFIG.POLL_INTERVAL);
  $('clearBtn').addEventListener('click', clearLog);
  addLog('info', 'System started', 'Dashboard connected. Waiting for ESP32 data.');
});

// ── Poll ──────────────────────────────────────────────────────────────────────
async function poll() {
  const data = await fetchSensorData();
  if (!data) { setOffline(); return; }
  update(data);
}

// ── Main update ───────────────────────────────────────────────────────────────
function update(data) {
  const { flame, pump_active: pump, cooldown_left: cooldown = 0,
          flame_events: events = 0, uptime = 0, rssi = 0, heap = 0 } = data;

  // ── Timestamp ──
  set('lastUpdated', new Date().toLocaleTimeString());

  // ── Big status card ──
  const card = $('statusCard');
  if (flame) {
    card.className = 'status-card danger';
    set('statusEmoji', '🔥');
    set('statusTitle', 'FIRE DETECTED');
    set('statusSub',   'IR flame sensor triggered — water pump activated');
  } else if (pump) {
    card.className = 'status-card warning';
    set('statusEmoji', '💧');
    set('statusTitle', 'Extinguishing...');
    set('statusSub',   `Pump running. Cooldown: ${cooldown}s remaining`);
  } else {
    card.className = 'status-card safe';
    set('statusEmoji', '🛡️');
    set('statusTitle', 'All Clear');
    set('statusSub',   'No flame detected. System monitoring normally.');
  }

  // ── Nav pill ──
  const dot  = $('pillDot');
  const pill = $('pillText');
  if (flame) {
    dot.className  = 'pill-dot danger';
    pill.textContent = '🔥 FIRE';
  } else if (pump) {
    dot.className  = 'pill-dot warning';
    pill.textContent = '💧 Pump Active';
  } else {
    dot.className  = 'pill-dot safe';
    pill.textContent = 'System Safe';
  }

  // ── Quick stats ──
  const flameVal = $('valFlame');
  flameVal.textContent = flame ? '🔥 YES' : 'NO';
  flameVal.className   = 'stat-val ' + (flame ? 'red' : 'green');

  const pumpVal = $('valPump');
  pumpVal.textContent = pump ? 'ON' : 'OFF';
  pumpVal.className   = 'stat-val ' + (pump ? 'yellow' : 'green');

  set('valEvents', events);

  const u = uptime;
  set('valUptime', `${Math.floor(u/3600)}h${Math.floor((u%3600)/60)}m`);
  set('valRssi', rssi + ' dBm');

  // ── Flame sensor card ──
  const cFlame   = $('cardFlame');
  const flameBig = $('flameBig');
  const flameBadge = $('flameBadge');

  if (flame) {
    cFlame.className       = 'card danger';
    flameBig.textContent   = 'FLAME!';
    flameBig.className     = 'card-big red';
    flameBadge.textContent = 'ALERT';
    flameBadge.className   = 'card-badge danger';
    set('flameDesc', '⚠️ Open flame detected by IR sensor');
  } else {
    cFlame.className       = 'card';
    flameBig.textContent   = 'NO FLAME';
    flameBig.className     = 'card-big green';
    flameBadge.textContent = 'SAFE';
    flameBadge.className   = 'card-badge safe';
    set('flameDesc', 'No infrared radiation detected');
  }

  // ── Pump card ──
  const cPump   = $('cardPump');
  const pumpBig = $('pumpBig');
  const pumpBadge = $('pumpBadge');

  if (flame) {
    cPump.className       = 'card danger';
    pumpBig.textContent   = 'PUMPING';
    pumpBig.className     = 'card-big red';
    pumpBadge.textContent = 'ACTIVE';
    pumpBadge.className   = 'card-badge danger';
    set('pumpDesc', '💧 Dispensing water — extinguishing fire!');
  } else if (pump) {
    cPump.className       = 'card active';
    pumpBig.textContent   = 'COOLDOWN';
    pumpBig.className     = 'card-big yellow';
    pumpBadge.textContent = 'COOLDOWN';
    pumpBadge.className   = 'card-badge warning';
    set('pumpDesc', `Flame gone. Pump runs ${cooldown}s more.`);
  } else {
    cPump.className       = 'card';
    pumpBig.textContent   = 'OFF';
    pumpBig.className     = 'card-big green';
    pumpBadge.textContent = 'STANDBY';
    pumpBadge.className   = 'card-badge safe';
    set('pumpDesc', 'Pump is off. No fire detected.');
  }

  // ── Device card ──
  $('deviceBig').textContent = rssi + ' dBm';
  $('deviceBig').className   = 'card-big ' + (rssi > -70 ? 'green' : rssi > -85 ? 'yellow' : 'red');
  set('deviceDesc', `Heap: ${heap} KB · Uptime: ${Math.floor(u/60)}m`);
  $('deviceBadge').textContent = 'ONLINE';
  $('deviceBadge').className   = 'card-badge safe';

  // ── Log new events ──
  if (flame && !state.prevFlame) {
    addLog('danger', '🔥 Fire Detected — Pump Activated',
           'IR sensor triggered. Relay closed. Water pump running.');
    toast('🔥 Fire detected! Pump ON', 'danger');
  }
  if (!flame && !pump && state.prevPump) {
    addLog('info', '✅ System Back to Normal',
           'Flame extinguished. Pump off. All clear.');
    toast('✅ All clear — pump off', 'success');
  }

  state.prevFlame  = flame;
  state.prevPump   = pump;
}

// ── Offline ───────────────────────────────────────────────────────────────────
function setOffline() {
  $('statusCard').className = 'status-card';
  set('statusEmoji', '⚠️');
  set('statusTitle', 'No Connection');
  set('statusSub',   'Cannot reach ESP32. Check device and network.');
  $('pillDot').className = 'pill-dot';
  set('pillText', 'Offline');
  $('deviceBadge').textContent = 'OFFLINE';
  $('deviceBadge').className   = 'card-badge danger';
}

// ── Log ───────────────────────────────────────────────────────────────────────
function addLog(type, title, detail) {
  const box   = $('logBox');
  const empty = $('logEmpty');
  if (empty) empty.style.display = 'none';

  const el = document.createElement('div');
  el.className = `log-item ${type}`;
  el.innerHTML = `
    <span class="log-time">${new Date().toLocaleTimeString()}</span>
    <div class="log-msg">
      <strong>${title}</strong>
      <span>${detail}</span>
    </div>
    <span class="log-tag ${type}">${type.toUpperCase()}</span>
  `;
  box.insertBefore(el, box.firstChild);

  // Keep max 50 entries
  const items = box.querySelectorAll('.log-item');
  if (items.length > 50) items[items.length - 1].remove();
}

function clearLog() {
  const box = $('logBox');
  box.querySelectorAll('.log-item').forEach(i => i.remove());
  $('logEmpty').style.display = 'flex';
  toast('Log cleared', 'success');
}

// ── Toast ─────────────────────────────────────────────────────────────────────
function toast(msg, type = '') {
  const t = $('toast');
  t.textContent = msg;
  t.className = `toast show ${type}`;
  clearTimeout(t._t);
  t._t = setTimeout(() => t.classList.remove('show'), 3000);
}
