/**
 * FireGuard — Mini sparkline chart for hero device mockup
 * Only the hero screen uses a chart (flame event timeline).
 */

const Charts = (() => {
  const flameHistory = [];   // 1 = flame, 0 = safe
  const MAX = 24;

  function init() {
    // nothing to init — drawn on demand
  }

  function pushData(flame) {
    flameHistory.push(flame ? 1 : 0);
    if (flameHistory.length > MAX) flameHistory.shift();
    drawMiniChart();
  }

  function drawMiniChart() {
    const container = document.getElementById('miniChart');
    if (!container) return;

    let canvas = container.querySelector('canvas');
    if (!canvas) {
      canvas = document.createElement('canvas');
      canvas.style.width  = '100%';
      canvas.style.height = '100%';
      container.appendChild(canvas);
    }

    canvas.width  = container.offsetWidth || 220;
    canvas.height = 50;

    const ctx  = canvas.getContext('2d');
    const w    = canvas.width;
    const h    = canvas.height;
    const data = flameHistory;

    ctx.clearRect(0, 0, w, h);
    if (data.length < 2) return;

    const step = w / (data.length - 1);

    // Draw grid line
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Draw flame bars (tall = flame, short = safe)
    data.forEach((val, i) => {
      const x = i * step;
      const barH = val ? h * 0.75 : h * 0.15;
      const y = h - barH;
      ctx.fillStyle = val ? 'rgba(255,107,53,0.8)' : 'rgba(0,230,118,0.4)';
      ctx.fillRect(x - 2, y, 4, barH);
    });
  }

  return { init, pushData };
})();
