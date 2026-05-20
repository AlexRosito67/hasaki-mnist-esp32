# xyron-mnist-esp32

MNIST handwritten digit recognition running entirely on an ESP32-C3 Super Mini, using a neural network header exported by [Xyron](https://github.com/AlexRosito67/Xyron-code).

No frameworks. No runtime. Just a C header and a microcontroller.

---

## Demo

Draw a digit on any browser or phone. The ESP32 runs inference locally and returns the prediction in real time.

---

## Hardware

- ESP32-C3 Super Mini
- LED matrix display (optional) — receives the predicted digit via HTTP GET

---

## How it works

1. ESP32 connects to WiFi and starts a web server
2. Any device on the same network opens `http://<ESP32_IP>`
3. Draw a digit on the canvas — auto-centered before inference
4. 784 floats sent via HTTP POST to `/predict`
5. ESP32 runs `predict()` from `mnist_int8.h` — no cloud, no external library
6. Prediction and confidence returned to browser
7. Result also sent to LED matrix display (if configured)

---

## Dependencies

Install via Arduino Library Manager:

- **ArduinoJson** v6+ — [arduinojson.org](https://arduinojson.org/)

Board support:

- ESP32 by Espressif — add in Arduino IDE via Boards Manager

---

## Setup

1. Open `mnist_esp32.ino` in Arduino IDE
2. Set your credentials at the top of the file:

```cpp
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define DISPLAY_IP      "192.168.1.XXX"   // optional
```

3. Copy `mnist_int8.h` to the same folder as `mnist_esp32.ino`
4. Select board: **ESP32C3 Dev Module**
5. Flash to ESP32-C3

---

## Finding the ESP32 IP

Open Serial Monitor at **115200 baud** after flashing.  
The ESP32 prints its IP on startup:

```
Connecting to YOUR_SSID....
Connected!
ESP32 IP: 192.168.1.55 (may be different for you)
WebServer ready on port 80
```

Alternatively, check your router's connected devices list.

---

## Usage

Open `http://<ESP32_IP>` on any browser — desktop, phone, or tablet on the same WiFi network.

- Draw a digit (0–9) on the canvas
- Click **Send to ESP32**
- Prediction and confidence appear instantly

**Tip:** The canvas auto-centers your digit before inference for best accuracy.

---

## Model

Trained with **Xyron v3.0.1**:

| Parameter | Value |
|-----------|-------|
| Architecture | 784 → 128 → 10 |
| Activations | ReLU + Softmax |
| Quantization | INT8 |
| Training set | 60,000 samples (MNIST) |
| Validation accuracy | ~98% |
| Header size | ~414 KB |
| Flash usage | ~78% on ESP32-C3 (1.3MB flash) |

---

## Trained with Xyron

```bash
# Train
xyron -d 784,128,10 -act relu,softmax -a train \
      -f mnist_train.csv -test-file mnist_test.csv \
      -e 1000 -l 0.001 --adam -o mnist_model.txt

# Export INT8 header
xyron -d 784,128,10 -act relu,softmax -a export \
      -m mnist_model.txt -o mnist_int8.h -q int8
```

Xyron will be available soon

---

## License

MIT
