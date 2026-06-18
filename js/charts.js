/**
 * FireGuard — Lightweight sparkline charts (no dependencies)
 * Draws mini line charts on canvas elements using raw Canvas API.
 */

const Charts = (() => {
  const HISTORY_SIZE = 30;

  const series = {
    temp:  { data: [], color: '#ff6b35', canvas: null, ctx: null },
    smoke: { data: [], color: '#ffab00', canvas: null, ctx: null },
    hum:   { data: [], color: '#3d8bff', canvas: null, ctx: null },
  };

  function init() {
    series.temp.canvas  = document.getElementById('tempChart');
    series.smoke.canvas = document.getElementById('smokeChart');
    series.hum.canvas   = document.getElementById('humChart');

    for (const key of Object.keys(series)) {
      const s = series[key];
      if (s.canvas) {
        s.canvas.width  = s.canvas.offsetWidth || 240;
        s.canvas.height = 60;
        s.ctx = s.canvas.getContext('2d');
      }
    }

    window.addEventListener('resize', () => {
      for (const key of Object.keys(series)) {
        const s = series[key];
        if (s.canvas) {
          s.canvas.width = s.canvas.offsetWidth || 240;
          drawChart(key);
        }
      }
    });
  }

  function pushData(temp, smoke, hum) {
    push('temp',  temp);
    push('smoke', smoke);
    push('hum',   hum);
    drawChart('temp');
    drawChart('smoke');
    drawChart('hum');
    drawMiniChart(temp, smoke);
  }

  function push(key, value) {
    const s = series[key];
    s.data.push(value);
    if (s.data.length > HISTORY_SIZE) s.data.shift();
  }

  function drawChart(key) {
    const s = series[key];
    if (!s.ctx || s.data.length < 2) return;

    const { ctx, data, color } = s;
    const w = s.canvas.width;
    const h = s.canvas.height;
    const pad = 4;

    ctx.clearRect(0, 0, w, h);

    const min = Math.min(...data);
    const max = Math.max(...data);
    const range = max - min || 1;

    const xStep = (w - pad * 2) / (data.length - 1);

    // Gradient fill
    const gradient = ctx.createLinearGradient(0, 0, 0, h);
    gradient.addColorStop(0, color + '40');
    gradient.addColorStop(1, color + '05');

    ctx.beginPath();
    ctx.moveTo(pad, h - pad - ((data[0] - min) / range) * (h - pad * 2));
    for (let i = 1; i < data.length; i++) {
      const x = pad + i * xStep;
      const y = h - pad - ((data[i] - min) / range) * (h - pad * 2);
      ctx.lineTo(x, y);
    }

    // Fill area
    ctx.lineTo(pad + (data.length - 1) * xStep, h - pad);
    ctx.lineTo(pad, h - pad);
    ctx.closePath();
    ctx.fillStyle = gradient;
    ctx.fill();

    // Stroke line
    ctx.beginPath();
    ctx.moveTo(pad, h - pad - ((data[0] - min) / range) * (h - pad * 2));
    for (let i = 1; i < data.length; i++) {
      const x = pad + i * xStep;
      const y = h - pad - ((data[i] - min) / range) * (h - pad * 2);
      ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.lineJoin = 'round';
    ctx.stroke();

    // Current value dot
    const lastX = pad + (data.length - 1) * xStep;
    const lastY = h - pad - ((data[data.length - 1] - min) / range) * (h - pad * 2);
    ctx.beginPath();
    ctx.arc(lastX, lastY, 3, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();
  }

  // Mini sparkline for the hero device mockup
  const miniData = [];
  function drawMiniChart(temp) {
    miniData.push(temp);
    if (miniData.length > 20) miniData.shift();

    const container = document.getElementById('miniChart');
    if (!container) return;

    let canvas = container.querySelector('canvas');
    if (!canvas) {
      canvas = document.createElement('canvas');
      canvas.style.width = '100%';
      canvas.style.height = '100%';
      container.appendChild(canvas);
    }
    canvas.width = container.offsetWidth || 200;
    canvas.height = 50;

    const ctx = canvas.getContext('2d');
    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);
    if (miniData.length < 2) return;

    const min = Math.min(...miniData);
    const max = Math.max(...miniData);
    const range = max - min || 1;
    const step = w / (miniData.length - 1);

    ctx.beginPath();
    for (let i = 0; i < miniData.length; i++) {
      const x = i * step;
      const y = h - ((miniData[i] - min) / range) * (h * 0.8) - h * 0.1;
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.strokeStyle = '#ff6b35';
    ctx.lineWidth = 1.5;
    ctx.stroke();
  }

  return { init, pushData };
})();
