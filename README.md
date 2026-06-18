# 🔥 FireGuard — IoT Fire Detection System

Real-time fire detection dashboard built with ESP32 + IR Flame Sensor + Relay (Water Pump), deployed on Vercel.

## Hardware

| Component | ESP32 Pin |
|-----------|-----------|
| IR Flame Sensor DO | GPIO 27 |
| Relay Module IN | GPIO 26 |
| Onboard LED | GPIO 2 |

**Relay wiring:**
- Relay `COM` → Water Pump (+)
- Relay `NO` → Power Supply (+)
- Power Supply (−) → Water Pump (−)

## How It Works

1. IR flame sensor detects fire → digital pin goes LOW
2. ESP32 triggers relay HIGH → water pump activates instantly
3. Pump stays on 10s after flame gone (cooldown)
4. ESP32 sends status to Vercel `/api/sensors` proxy
5. Dashboard updates live from anywhere in the world

## Deploy to Vercel

### 1. Push to GitHub
```bash
git add .
git commit -m "FireGuard initial commit"
git push origin main
```

### 2. Import in Vercel
- Go to [vercel.com](https://vercel.com) → New Project → Import your GitHub repo

### 3. Set Environment Variable
In Vercel project settings → Environment Variables:

| Key | Value |
|-----|-------|
| `ESP32_URL` | `http://YOUR_ESP32_IP/api/sensors` |

> Without `ESP32_URL`, the dashboard runs in **demo mode** with simulated data.

### 4. ESP32 Firmware
- Open `esp32/fireguard.ino` in Arduino IDE
- Set your WiFi SSID and password
- Install library: **ArduinoJson** (v6) via Library Manager
- Flash to ESP32
- Open Serial Monitor — it will print the IP address

## Local Development

```bash
npm install -g vercel
vercel dev
```

Visit `http://localhost:3000`

## Project Structure

```
FireGuard/
├── api/
│   ├── sensors.js     ← Vercel serverless proxy → ESP32
│   └── status.js      ← Device health endpoint
├── public/
│   ├── index.html     ← Dashboard UI
│   ├── css/style.css
│   └── js/
│       ├── data.js    ← Fetches /api/sensors
│       ├── charts.js  ← Canvas sparkline chart
│       └── app.js     ← Dashboard logic
├── esp32/
│   └── fireguard.ino  ← ESP32 Arduino firmware
├── vercel.json
├── package.json
└── .env.example
```
