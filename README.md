# PINCHSERVO

Browser hand-gesture controller for an ESP32-driven SG90 servo. MediaPipe runs entirely in the browser; the ESP32 only serves `/` and `/set?angle=`.

## Files

- `index.html` – web app (vanilla JS, MediaPipe Tasks Vision 0.10.14 pinned)
- `PINCHSERVO_ESP32.ino` – firmware

## Wiring

| Component | ESP32 |
|---|---|
| SG90 signal (orange) | GPIO 18 |
| SG90 power (red) | external 5 V supply + |
| SG90 ground (brown) | external supply − AND ESP32 GND (common ground) |
| LCD SDA | GPIO 21 |
| LCD SCL | GPIO 22 |
| LCD VCC / GND | 5 V (VIN) / GND |
| LCD I2C address | 0x27 (change `LCD_ADDRESS` if your backpack differs, often 0x3F) |

Do not power the servo from the ESP32 3.3 V pin. Pins, address, pulse range and start angle are constants at the top of the sketch.

## Arduino setup

Board package: esp32 by Espressif. Board: ESP32 Dev Module.

Libraries (Library Manager): `ESP32Servo` and `LiquidCrystal I2C` (Frank de Brabander). `WiFi`, `WebServer` and `Wire` ship with the board package. If `LiquidCrystal I2C` fails to compile on a newer core, install an ESP32-compatible fork of it.

Edit `WIFI_SSID` and `WIFI_PASSWORD`, upload, and read the IP from LCD line 2 (also on Serial at 115200). The ESP32 and the browser device must be on the same network. Consider a DHCP reservation so the IP stays fixed.

## API

- `GET /` → plain text `PINCHSERVO READY`
- `GET /set?angle=90` → `{"status":"ok","angle":90}`
- Invalid or out-of-range angle → HTTP 400 `{"status":"error","message":"..."}`
- CORS headers on every response and `OPTIONS` handled.

## Deploy on Render

1. Push this folder to a GitHub repository (files at the repo root).
2. Render dashboard → New → Static Site → connect the repository.
3. Branch: `main`. Build Command: leave empty. Publish Directory: `.`
4. Create Static Site. Open the generated `https://….onrender.com` URL.

## HTTPS site → HTTP ESP32 (the core problem)

The site is HTTPS, the ESP32 is plain HTTP. Browsers block that as mixed content unless a local-network exemption applies.

- Chrome / Edge 142+: the page uses an IP literal plus `targetAddressSpace: "local"`. The browser shows a "Local network access" prompt; after Allow, the request is exempt from mixed-content blocking. If you pressed Block, reset it under the padlock → Site settings.
- Firefox: blocks http from https pages as mixed content, with no local-network prompt as of the versions reported. Workarounds: padlock/shield → disable protection for this site, or `about:config` → `security.mixed_content.block_active_content` = false (lowers security globally; revert afterwards).
- Safari / iOS browsers: expect blocking; no workaround verified.
- Android Chrome: same code path as desktop Chrome, but not verified here. Test it.
- Camera needs a secure context, so the page itself cannot simply be served over HTTP from the ESP32 (except via `localhost`).

Fallbacks when the browser refuses:

1. Run the same `index.html` locally: in the project folder run `python -m http.server 8000` and open `http://localhost:8000`. `localhost` is a secure context (camera works) and not HTTPS, so there is no mixed content. Not verified in every browser.
2. Use Chrome/Edge 142+ with the hosted site.
3. A relay (for example MQTT over secure WebSockets) works in every browser but needs a third-party or self-hosted broker and a firmware change. Not implemented, because it introduces a cloud dependency.

## Usage

1. Open the site, enter the ESP32 IP, press Connect, allow local network access.
2. Start Camera, allow the camera. Hand tracking takes over the servo automatically; untick the switch for manual control.
3. Adjust Minimum/Maximum distance so a pinch gives ~0° and a wide spread ~180°.

Pinched fingers give a lower angle; open fingers give a higher angle. If the hand disappears, the last angle is held.

## Testing checklist

- [ ] LCD shows PINCHSERVO READY and the IP
- [ ] `http://<ip>/` returns PINCHSERVO READY
- [ ] `http://<ip>/set?angle=0`, `90`, `180` move the servo; `181`, `abc` and a missing angle return 400
- [ ] Hosted page: Connect succeeds and the local network prompt appears (Chrome/Edge)
- [ ] Manual slider moves the servo; Reset returns it to 90°
- [ ] Camera starts, landmarks align with the hand, the preview is mirrored
- [ ] Pinch lowers and opening raises the angle; the calibration sliders cannot cross
- [ ] Hand leaves the frame: the angle holds
- [ ] Stop Camera turns the camera light off
- [ ] Power-cycle the ESP32 or router: the page reconnects automatically
- [ ] FPS stays near 15 on your device

## Known limitations

- Direct HTTPS→HTTP depends on the browser (see above).
- No authentication: anyone on your LAN who can reach the ESP32 can move the servo.
- MediaPipe, its WASM and the model load from jsdelivr and Google storage CDNs, so the first load needs internet.
- CPU inference only; low-end phones may run below 15 FPS (the loop adapts its rate).
- Single hand, 0–180° mapping through the SG90 pulse range 500–2400 µs; adjust the range constants if your servo hits its end stops.
- The browser console may show a harmless MediaPipe "XNNPACK delegate" info message.
