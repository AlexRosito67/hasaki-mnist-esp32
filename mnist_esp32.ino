// ======================================================
//  MNIST Demo
//  ESP32-C3 Super Mini — MNIST Inference over WiFi
//  Receives 784 floats via POST /predict
//  Returns the predicted digit
//  Sends result to matrix display via HTTP GET (optional
// ======================================================

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mnist_int8.h"

// ----- CONFIGURATION -----
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define DISPLAY_IP      "192.168.1.XXX"   // Matrix display IP
#define DISPLAY_PORT    80
#define SERVER_PORT     80
// -------------------------

WebServer server(SERVER_PORT);

// Optional: remove or adapt to your own display
void sendToDisplay(int digit) {
  HTTPClient http;
  String url = "http://" + String(DISPLAY_IP) + ":" + String(DISPLAY_PORT) +
               "/&MSG=" + String(digit) + "/&";
  Serial.println("Sending to display: " + url);
  http.begin(url);
  int code = http.GET();
  Serial.println("Display responded: " + String(code));
  http.end();
}

// Embedded Canvas HTML  — served from the ESP32
const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MNIST ESP32</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: #1a1a1a; color: #e0e0e0;
      font-family: monospace;
      display: flex; flex-direction: column;
      align-items: center; justify-content: center;
      min-height: 100vh; gap: 1.2rem; padding: 2rem;
    }
    h1 { font-size: 1rem; letter-spacing: 3px; color: #888; text-transform: uppercase; }
    canvas#mnistCanvas {
      border: 1px solid #444; border-radius: 6px;
      cursor: crosshair; background: #000;
      touch-action: none; image-rendering: pixelated;
    }
    .controls { display: flex; gap: 1rem; align-items: center; flex-wrap: wrap; justify-content: center; }
    .controls label { font-size: 12px; color: #888; }
    input[type=range] { width: 80px; accent-color: #4af; }
    .buttons { display: flex; gap: 10px; flex-wrap: wrap; justify-content: center; }
    button {
      background: transparent; border: 1px solid #444; color: #ccc;
      padding: 8px 20px; font-size: 13px; font-family: monospace;
      border-radius: 4px; cursor: pointer;
    }
    button#sendBtn { border-color: #4af; color: #4af; }
    #previewCanvas { display: none; border: 1px solid #333; border-radius: 4px; image-rendering: pixelated; background: #000; }
    #statusBox { display: none; padding: 10px 20px; border-radius: 4px; font-size: 13px; text-align: center; min-width: 220px; }
    #pixelStats { font-size: 11px; color: #555; }
  </style>
</head>
<body>
  <h1>MNIST &rarr; ESP32</h1>
  <canvas id="mnistCanvas" width="280" height="280"></canvas>
  <div class="controls">
    <label>Brush</label>
    <input type="range" id="brushSize" min="8" max="24" value="14" step="1">
    <span id="brushOut" style="font-size:12px; min-width:20px;">14</span>
  </div>
  <div class="buttons">
    <button id="clearBtn">Clear</button>
    <button id="previewBtn">Preview 28x28</button>
    <button id="sendBtn">Send to ESP32</button>
  </div>
  <canvas id="previewCanvas" width="112" height="112"></canvas>
  <div id="pixelStats"></div>
  <div id="statusBox"></div>
<script>
const canvas = document.getElementById('mnistCanvas');
const ctx = canvas.getContext('2d');
const preview = document.getElementById('previewCanvas');
const pCtx = preview.getContext('2d');
const brushSlider = document.getElementById('brushSize');
const brushOut = document.getElementById('brushOut');
const statusBox = document.getElementById('statusBox');
const pixelStats = document.getElementById('pixelStats');
let drawing = false, lastX = 0, lastY = 0;
ctx.fillStyle = '#000'; ctx.fillRect(0, 0, 280, 280);

function getPos(e) {
  const r = canvas.getBoundingClientRect();
  const sx = canvas.width / r.width, sy = canvas.height / r.height;
  if (e.touches) return [(e.touches[0].clientX - r.left)*sx, (e.touches[0].clientY - r.top)*sy];
  return [(e.clientX - r.left)*sx, (e.clientY - r.top)*sy];
}
function drawAt(x, y, fx, fy) {
  const s = parseInt(brushSlider.value);
  ctx.lineWidth = s; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
  ctx.strokeStyle = '#fff'; ctx.shadowBlur = s*0.6; ctx.shadowColor = 'rgba(255,255,255,0.5)';
  ctx.beginPath(); ctx.moveTo(fx, fy); ctx.lineTo(x, y); ctx.stroke(); ctx.shadowBlur = 0;
}
canvas.addEventListener('mousedown', e => { drawing=true; [lastX,lastY]=getPos(e); drawAt(lastX,lastY,lastX,lastY); });
canvas.addEventListener('mousemove', e => { if(!drawing) return; const [x,y]=getPos(e); drawAt(x,y,lastX,lastY); [lastX,lastY]=[x,y]; });
canvas.addEventListener('mouseup', () => drawing=false);
canvas.addEventListener('mouseleave', () => drawing=false);
canvas.addEventListener('touchstart', e => { e.preventDefault(); drawing=true; [lastX,lastY]=getPos(e); drawAt(lastX,lastY,lastX,lastY); },{passive:false});
canvas.addEventListener('touchmove', e => { e.preventDefault(); if(!drawing) return; const [x,y]=getPos(e); drawAt(x,y,lastX,lastY); [lastX,lastY]=[x,y]; },{passive:false});
canvas.addEventListener('touchend', () => drawing=false);
brushSlider.addEventListener('input', () => brushOut.textContent = brushSlider.value);
document.getElementById('clearBtn').addEventListener('click', () => {
  ctx.fillStyle='#000'; ctx.fillRect(0,0,280,280);
  statusBox.style.display='none'; preview.style.display='none'; pixelStats.textContent='';
});

// MNIST style Auto-centered
function getPixelsCentered() {
  const raw = ctx.getImageData(0, 0, 280, 280).data;
  let minX=280, maxX=0, minY=280, maxY=0;
  for (let y=0; y<280; y++) for (let x=0; x<280; x++) {
    if (raw[(y*280+x)*4] > 10) {
      if (x<minX) minX=x; if (x>maxX) maxX=x;
      if (y<minY) minY=y; if (y>maxY) maxY=y;
    }
  }
  if (minX > maxX) return new Array(784).fill(0);
  const pad=20;
  minX=Math.max(0,minX-pad); maxX=Math.min(279,maxX+pad);
  minY=Math.max(0,minY-pad); maxY=Math.min(279,maxY+pad);
  const w=maxX-minX+1, h=maxY-minY+1;
  const tmp=document.createElement('canvas'); tmp.width=28; tmp.height=28;
  const tCtx=tmp.getContext('2d');
  tCtx.fillStyle='#000'; tCtx.fillRect(0,0,28,28);
  const scale=Math.min(20/w, 20/h);
  const sw=Math.round(w*scale), sh=Math.round(h*scale);
  const ox=Math.round((28-sw)/2), oy=Math.round((28-sh)/2);
  tCtx.drawImage(canvas, minX, minY, w, h, ox, oy, sw, sh);
  const data=tCtx.getImageData(0,0,28,28).data;
  return Array.from({length:784}, (_,i) => parseFloat((data[i*4]/255.0).toFixed(5)));
}

document.getElementById('previewBtn').addEventListener('click', () => {
  const pixels=getPixelsCentered();
  const nonZero=pixels.filter(p=>p>0.05).length;
  pCtx.fillStyle='#000'; pCtx.fillRect(0,0,112,112);
  for (let y=0; y<28; y++) for (let x=0; x<28; x++) {
    const v=Math.round(pixels[y*28+x]*255);
    pCtx.fillStyle=`rgb(${v},${v},${v})`; pCtx.fillRect(x*4,y*4,4,4);
  }
  preview.style.display='block';
  pixelStats.textContent=`active pixels: ${nonZero}/784 | auto-centrado`;
});

document.getElementById('sendBtn').addEventListener('click', async () => {
  const pixels=getPixelsCentered();
  if (pixels.filter(p=>p>0.05).length < 10) { showStatus('Canvas empty.','warn'); return; }
  showStatus('Sending to ESP32...','info');
  try {
    const res=await fetch('/predict', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({pixels})
    });
    if (res.ok) {
      const data=await res.json();
      const digit=data.digit??'?';
      const conf=data.confidence?(data.confidence*100).toFixed(1)+'%':'';
      showStatus(`Prediction: <span style="font-size:2rem;color:#4af;">${digit}</span> ${conf}`,'ok');
    } else showStatus(`Error HTTP ${res.status}`,'err');
  } catch(e) { showStatus('Could not connect to ESP32.','err'); }
});

function showStatus(msg, type) {
  const s={ok:'background:#0a2a1a;border:1px solid #2a6a3a;color:#4dbb6e;',
           err:'background:#2a0a0a;border:1px solid #6a2a2a;color:#bb4d4d;',
           warn:'background:#2a1a00;border:1px solid #6a4a00;color:#bbaa4d;',
           info:'background:#0a1a2a;border:1px solid #2a4a6a;color:#4d88bb;'};
  statusBox.style.cssText=(s[type]||s.info)+'display:block;padding:10px 20px;border-radius:4px;font-size:13px;text-align:center;';
  statusBox.innerHTML=msg;
}
</script>
</body>
</html>
)rawhtml";

// Handle GET / — serve the canvas HTML
void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "text/html", INDEX_HTML);
}

// Handle POST /predict
void handlePredict() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Empty body\"}");
    return;
  }

  String body = server.arg("plain");

  // Parse JSON — 784 floats require a large document
  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonArray arr = doc["pixels"].as<JsonArray>();
  if (arr.size() != 784) {
    server.send(400, "application/json", "{\"error\":\"784 pixels are expected\"}");
    return;
  }

  // Copy pixels to input buffer
  float input[784];
  for (int i = 0; i < 784; i++) {
    input[i] = arr[i].as<float>();
  }

  // Run inference
  float output[10];
  predict(input, output);

  // Find digit with highest probability
  int best = 0;
  float bestVal = output[0];
  for (int i = 1; i < 10; i++) {
    if (output[i] > bestVal) {
      bestVal = output[i];
      best = i;
    }
  }

  Serial.printf("Prediction: %d (confidence: %.2f%%)\n", best, bestVal * 100.0f);

  // Send response to browser
  String response = "{\"digit\":" + String(best) +
                    ",\"confidence\":" + String(bestVal, 4) + "}";
  server.send(200, "application/json", response);

  // Send result to matrix display
  sendToDisplay(best);
}

// Handle OPTIONS /predict (CORS preflight)
void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Connect to WiFi
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Register routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/predict", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    handlePredict();
  });
  server.on("/predict", HTTP_OPTIONS, handleOptions);

  // Ping route — health check
  server.on("/ping", HTTP_GET, []() {
    server.send(200, "text/plain", "pong");
  });

  server.begin();
  Serial.println("WebServer ready on port " + String(SERVER_PORT));
}

void loop() {
  server.handleClient();
}
