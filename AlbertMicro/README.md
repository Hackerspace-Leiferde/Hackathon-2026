# AlbertMicro 🤖

A tiny ESP32-S3 powered quadruped robot with a personality. It walks on four
servo legs, shows expressive **RoboEyes** on an OLED display, and can flip into
an **info screen** that shows the time, live weather, and a YouTube subscriber
count. You drive it over **Bluetooth LE** (or the Serial Monitor) with simple
text commands. Built as a VS Code + PlatformIO project (ported from an
original Arduino sketch).

---

## Features

- 🦿 **4-servo walking gait** — forward, backward, strafe left/right, spin in place
- 😀 **Animated eyes** (FluxGarage RoboEyes) with moods: happy, angry, tired, curious
- 🤸 **Trick poses** — push-ups, swing, gallop, sit, stand, lie down
- 🕒 **Info screen** — clock (NTP), live weather icon + temperature (OpenWeatherMap), YouTube subscriber count
- 📡 **Bluetooth LE UART** control (Nordic UART Service) + USB Serial control

---

## Hardware

| Part | Notes |
|---|---|
| **Seeed XIAO ESP32-S3** (or any ESP32-S3 with BLE + WiFi) | `env:seeed_xiao_esp32s3` in `platformio.ini` |
| **4× micro servos** (e.g. SG90) | One per leg |
| **0.96" 128×64 OLED, SSD1306 driver, I²C** | Address `0x3C` (some modules use `0x3D` — check the solder jumper on the back, usually marked `0x78`/`0x7A`, the 8-bit equivalents) |
| Battery / 5 V supply | Servos draw current — power them separately from the ESP32 logic if you can |
| 3D-printed body, legs & feet | STL files / assembly photos kept outside this repo — ask in the team channel if you need them |

### ⚠️ XIAO ESP32-S3: board "D" labels ≠ GPIO numbers

The code addresses pins by **raw GPIO number**, but the silkscreen on the
XIAO ESP32-S3 board prints **`D0`–`D10`** labels that are offset from the
GPIO number. Mixing these up is the single most common wiring bug on this
board — always translate through this table:

| Board silkscreen | GPIO | Used in this project for |
|---|---|---|
| *(no header pin — onboard BOOT button)* | GPIO0 | *(unused — avoid; it's a boot-strapping pin)* |
| `D0` | GPIO1 | Servo 0 — front-left |
| `D1` | GPIO2 | Servo 1 — front-right |
| `D2` | GPIO3 | Servo 2 — back-left |
| `D3` | GPIO4 | Servo 3 — back-right |
| `D4` (printed "SDA") | GPIO5 | *(unused — I²C runs on D9/D10 instead, see below)* |
| `D5` (printed "SCL") | GPIO6 | *(unused)* |
| `D6` | GPIO43 | free |
| `D7` | GPIO44 | free |
| `D8` | GPIO7 | free |
| `D9` (printed "MISO") | GPIO8 | OLED SDA |
| `D10` (printed "MOSI") | GPIO9 | OLED SCL |

Note the OLED is wired to `D9`/`D10` (labeled MISO/MOSI on the board), **not**
the pins printed "SDA"/"SCL" (those are `D4`/`D5` = GPIO5/6) — the ESP32's I²C
peripheral can run on any GPIO pair, and this project's pin defines
(`I2C_SDA`/`I2C_SCL` in [`src/main.cpp`](src/main.cpp)) pick D9/D10 on purpose
to leave the SDA/SCL-labeled pins free.

### Wiring summary

```
OLED  SDA  -> D9  (GPIO8)
OLED  SCL  -> D10 (GPIO9)
OLED  VCC  -> 3V3
OLED  GND  -> GND

Servo 0 (front-left)  -> D0 (GPIO1)
Servo 1 (front-right) -> D1 (GPIO2)
Servo 2 (back-left)   -> D2 (GPIO3)
Servo 3 (back-right)  -> D3 (GPIO4)
Servo VCC -> 5V (external supply recommended)
Servo GND -> common GND with ESP32
```

> The exact leg-to-servo mapping depends on how you assemble the body. If a
> command makes it walk backwards or crab sideways, swap the servo connectors
> or tweak the `ampScale[]` arrays in the `loop()` switch statement in
> [`src/main.cpp`](src/main.cpp).

---

## Software setup

### 1. Open the project in VS Code
Install the **PlatformIO IDE** extension, then open this folder in VS Code.
PlatformIO reads [`platformio.ini`](platformio.ini) and installs all required
libraries automatically on first build:

- ESP32Servo
- ArduinoJson
- Adafruit GFX Library
- Adafruit SSD1306
- FluxGarage RoboEyes

(WiFi, HTTPClient, Wire, time, and the BLE libraries ship with the
`espressif32` Arduino framework, no extra `lib_deps` entry needed.)

### 2. Configure your secrets
Copy the template and fill in your own values:

```sh
cp include/Secrets.h.example include/Secrets.h
```

Then edit [`include/Secrets.h`](include/Secrets.h):

```cpp
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"
#define WEATHER_API_KEY   "YOUR_OPENWEATHERMAP_API_KEY"
#define WEATHER_CITY      "Rome,IT"            // City,CountryCode
#define YOUTUBE_API_KEY   "YOUR_YOUTUBE_API_KEY"
#define YOUTUBE_CHANNEL   "YOUR_YOUTUBE_CHANNEL_ID"
```

`include/Secrets.h` is listed in [`.gitignore`](.gitignore), so your real
credentials never get committed — only `Secrets.h.example` is tracked.

You can adjust the timezone in [`src/main.cpp`](src/main.cpp):

```cpp
#define GMT_OFFSET_SEC    3600   // base UTC offset in seconds (3600 = UTC+1)
#define DAYLIGHT_SEC      3600   // daylight-saving offset in seconds
```

### 3. Build & upload
Use the PlatformIO toolbar in VS Code, or from the terminal:

```sh
pio run                # build
pio run -t upload      # build + flash
pio device monitor      # serial monitor @ 115200 baud
```

---

## Getting the API keys

### 🌤️ OpenWeatherMap (weather)

1. Go to <https://openweathermap.org/> and **create a free account**.
2. After signing in, open **My profile → API keys** (or visit
   <https://home.openweathermap.org/api_keys>).
3. Copy the **default key** (or click *Generate* to make a new one).
4. Paste it into `WEATHER_API_KEY` in `include/Secrets.h`.
5. Set `WEATHER_CITY` to your location in the form `City,CountryCode`
   (e.g. `London,GB`, `Rome,IT`, `Austin,US`).

> 🔑 A brand-new OpenWeatherMap key can take **a few hours to activate**. If you
> get an HTTP `401` in the Serial Monitor right after signing up, wait and try
> again later. This project uses the free **Current Weather Data** endpoint.

### ▶️ YouTube Data API v3 (subscriber count)

1. Open the **Google Cloud Console**: <https://console.cloud.google.com/>.
2. Create a new project (top bar → project dropdown → *New Project*).
3. Enable the API: go to **APIs & Services → Library**, search for
   **"YouTube Data API v3"**, and click **Enable**.
4. Create the key: **APIs & Services → Credentials → Create Credentials →
   API key**. Copy it into `YOUTUBE_API_KEY`.
5. *(Recommended)* Click the key → **Restrict key** → under *API restrictions*
   limit it to **YouTube Data API v3** so it can't be abused elsewhere.
6. Find your **Channel ID**:
   - Sign in to YouTube → **Settings → Advanced settings**, where your channel
     ID is shown, **or**
   - Open your channel page and copy the ID from the URL
     `https://www.youtube.com/channel/UCxxxxxxxxxxxxxxxxxxxxxx`
     (the part starting with `UC...`).
   - Paste it into `YOUTUBE_CHANNEL`.

> 🔑 The YouTube Data API has a daily **quota** (10,000 units/day by default).
> Reading channel statistics costs ~1 unit per call, and this sketch only polls
> every 5 minutes, so you'll stay well within the free tier.

---

## Controlling the robot

Send any of the commands below either from the **PlatformIO Serial Monitor**
(115200 baud, newline ending) or from a **BLE UART app** on your phone.

### Connect over Bluetooth

The robot advertises as **`AlbertMini`** using the Nordic UART Service.
Use any BLE UART terminal app, for example:

- **nRF Connect** (Android / iOS)
- **Serial Bluetooth Terminal** (Android)
- **Bluefruit Connect** (iOS / Android) → *UART* mode

Scan, connect to **AlbertMini**, open the UART/terminal view, and type a command.

### Command reference

| Command | Action |
|---|---|
| `WALK`    | Walk forward |
| `BACK`    | Walk backward |
| `LEFT`    | Strafe / turn left |
| `RIGHT`   | Strafe / turn right |
| `SL`      | Spin left in place |
| `SR`      | Spin right in place |
| `STOP`    | Stop moving (idle) |
| `UP`      | Stand up (legs centered) |
| `DOWN`    | Lie down |
| `REST`    | Rest pose, sleepy eyes, screen off |
| `INFO`    | Sit + show clock / weather / YouTube info screen |
| `PUSHUPS` | Do push-ups (angry eyes) |
| `SWING`   | Swing dance |
| `GALLOP`  | Gallop animation |
| `SCAN`    | Re-run the I²C bus scan (debug — see Troubleshooting) |

Commands are case-insensitive. On boot the robot starts in **REST**.

---

## Info screen layout

When you send `INFO`, the OLED shows:

```
12:34      ☀         <- time (NTP) + weather icon
21C  Clear           <- temperature + condition
---------------------
▶  12345             <- YouTube subscriber count
```

Weather icons are drawn for: **clear, clouds, rain/drizzle, snow,
thunderstorm** (anything else falls back to a cloud).

---

## Tuning notes

- **Gait feel** — tweak `FREQUENCY`, `AMPLITUDE`, `off_set_walk[]`, and the
  per-mode `ampScale[]` arrays in `loop()` in [`src/main.cpp`](src/main.cpp).
- **Smoothness of poses** — `MOVE_STEPS`, `SPEED_FACTOR`, `MAX_STEP`, `DELAY_TIME`.
- **Servo limits** — `SERVOMIN` / `SERVOMAX` clamp every write to keep servos safe.
- **Update intervals** — `WEATHER_INTERVAL` (10 min) and `YT_INTERVAL` (5 min).

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Display stays black, but `SCAN` finds a device at `0x3C`/`0x3D` | Wrong driver class — most 0.96" OLEDs are **SSD1306**, not SH1106, even though both answer on the same I²C address. This project already uses `Adafruit_SSD1306`; if you swap the panel, double check. |
| `SCAN` finds no I²C device at all | Wiring on the wrong pins. OLED must be on `D9`/`D10` (GPIO8/9), *not* the pins printed "SDA"/"SCL" (those are GPIO5/6) — see the pin table above. |
| One servo doesn't move, others fine (confirmed by swapping servos between pins) | Board-pin-to-GPIO mismatch. Check `SERVO_PINS[]` in `src/main.cpp` against the pin table above — `D0..D3` are GPIO`1..4`, not GPIO`0..3`. |
| `WiFi failed — continuing offline` | Wrong SSID/password, or 5 GHz-only network (ESP32 needs 2.4 GHz) |
| Weather HTTP `401` | API key not active yet (wait a few hours) or wrong key |
| YouTube HTTP `403` | API not enabled, quota exceeded, or key restricted incorrectly |
| Servos jitter / browns out | Power servos from a separate 5 V supply with common ground |
| Robot walks wrong direction | Swap servo connectors or invert the relevant `ampScale[]` signs |
| Build fails with `intelhex` / esptool import error | Run `pip install intelhex` inside PlatformIO's Python env (`~/.platformio/penv/bin/python -m pip install intelhex`) |

---

## Credits

- Eyes animation: **FluxGarage RoboEyes** library
- Weather data: **OpenWeatherMap**
- Subscriber stats: **YouTube Data API v3**
