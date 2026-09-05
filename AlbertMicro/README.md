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
- 🎙️ **Voice control** — say "Hi ESP" + a command phrase into the onboard mic (offline, no cloud/internet needed — see [Voice control](#voice-control))

---

## Hardware

| Part | Notes |
|---|---|
| **Seeed XIAO ESP32-S3 *Sense*** | Needs the **Sense** variant specifically — its camera/mic expansion board is where the PDM microphone for [voice control](#voice-control) lives. The plain XIAO ESP32-S3 has no onboard mic. `env:seeed_xiao_esp32s3` in `platformio.ini` covers both. |
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
| *(no header pin — on the Sense expansion board)* | GPIO41 | Onboard PDM mic — data |
| *(no header pin — on the Sense expansion board)* | GPIO42 | Onboard PDM mic — clock |

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

Onboard PDM mic (Sense expansion board — no wiring needed, already connected)
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

(WiFi, HTTPClient, Wire, time, BLE, `ESP_I2S` and `ESP_SR` all ship with the
Arduino framework core itself, no extra `lib_deps` entry needed.)

> ⚠️ **Platform note:** `platformio.ini` points `platform` at a pinned
> [**pioarduino**](https://github.com/pioarduino/platform-espressif32)
> release zip instead of the plain `espressif32` registry entry. Voice
> control ([`ESP_SR`](#voice-control)) needs Arduino-ESP32 core 3.3.x, which
> the official PlatformIO registry doesn't ship yet — pioarduino is a
> community fork that tracks newer core releases, same `platform_packages`
> API otherwise. First install downloads a full new toolchain/framework and
> can take a few minutes.

On first upload, a PlatformIO post-action ([`flash_sr_models.py`](flash_sr_models.py))
automatically flashes the ~3.2 MB speech-recognition model file
(`srmodels.bin`, bundled with the framework — not part of this repo) to the
`model` partition right after the firmware. This adds maybe 10–20 seconds to
the first `pio run -t upload`; you'll see a
`Flashing speech-recognition model...` line in the terminal. It re-flashes
this every upload — harmless, just a bit of extra time per upload.

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
| `SIT`     | Sit (front legs up, rear legs down) |
| `REST`    | Rest pose, sleepy eyes, screen off |
| `INFO`    | Sit + show clock / weather / YouTube info screen |
| `PUSHUPS` | Do push-ups (angry eyes) |
| `SWING`   | Swing dance |
| `GALLOP`  | Gallop animation |
| `SCAN`    | Re-run the I²C bus scan (debug — see Troubleshooting) |

Commands are case-insensitive. On boot the robot starts in **REST**.

---

## Voice control

Albert also listens on the **onboard PDM microphone** (XIAO ESP32-S3 *Sense*
only) using Espressif's offline **ESP-SR** speech recognition — no WiFi,
no cloud, no internet needed, everything runs on-chip.

**How it works:** say the wake word **"Hi ESP"**, wait for it to start
listening (a few seconds), then say one of the command phrases below. Each
phrase runs the exact same code path as typing the matching command over
BLE/Serial — same eyes, same gait engine, same everything. After a command
(or a few seconds of silence), it goes back to waiting for the wake word.

> 🔒 **Why "Hi ESP" and not a custom wake word?** ESP-SR ships a whole
> catalog of pre-trained wake words ("Alexa", "Jarvis", "Computer", …), but
> the prebuilt model file this Arduino library bundles only includes **one**
> of them — "Hi ESP" — to keep it a reasonable size. Picking a different one
> (or a fully custom "Hey Albert") means building ESP-SR from source with a
> different config, i.e. a full ESP-IDF project instead of this simple
> Arduino sketch — out of scope here, but see the
> [esp-sr repo](https://github.com/espressif/esp-sr) if you want to go down
> that road later.

### Voice command reference

Command *phrases* are plain English text — no training needed, ESP-SR
converts them to phonemes at boot. Feel free to edit/add phrases in the
`sr_commands[]` table in [`src/main.cpp`](src/main.cpp) (search for
"VOICE CONTROL"); each entry is `{command, "phrase to say"}`.

"Sit down", "Lie down", "Walk" and "Dance" are the phrases from the original
TechTalkies voice-dog project this was merged from. "Good boy" and "Stretch"
are that project's other two phrases, repurposed to the closest matching
moves Albert actually has (rest / stand) since Albert has no wag or stretch
animation. Everything else is new, covering the rest of Albert's command set.

| Say "Hi ESP", then... | Same as typed command | Action |
|---|---|---|
| "Walk"        | `WALK`    | Walk forward |
| "Go back"     | `BACK`    | Walk backward |
| "Turn left"   | `LEFT`    | Strafe / turn left |
| "Turn right"  | `RIGHT`   | Strafe / turn right |
| "Spin left"   | `SL`      | Spin left in place |
| "Spin right"  | `SR`      | Spin right in place |
| "Stop"        | `STOP`    | Stop moving |
| "Stretch"     | `UP`      | Stand up |
| "Sit down"    | `SIT`     | Sit |
| "Lie down"    | `DOWN`    | Lie down |
| "Good boy"    | `REST`    | Rest pose, sleepy eyes, screen off |
| "Show info"   | `INFO`    | Sit + show clock/weather/YouTube info screen |
| "Push ups"    | `PUSHUPS` | Do push-ups |
| "Dance"       | `SWING`   | Swing dance |
| "Gallop"      | `GALLOP`  | Gallop animation |

### Voice troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Nothing happens when saying "Hi ESP" | Confirm it's a **Sense** board (mic present) and check Serial Monitor for `ERROR: onboard PDM microphone init failed` at boot |
| Wake word triggers but commands are never recognized | Speak clearly a beat after the wake word, close to the mic, in a reasonably quiet room — offline command recognition is less forgiving than cloud assistants |
| Wrong command triggers | Try rephrasing closer to the exact phrase text in the table above, or add your own alternate phrasing for that command in `sr_commands[]` |
| First upload is slow / seems stuck after "Flashing speech-recognition model..." | Expected — it's writing a ~3.2 MB file. Let it finish; don't unplug |

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
- Voice control: **Espressif ESP-SR** (WakeNet + MultiNet), via the `ESP_SR`
  Arduino library — command-phrase idea merged in from
  [TechTalkies' "100 Pet Dog"](https://github.com/TechTalkies/YouTube/tree/main/100%20Pet%20Dog)
  voice-controlled robot dog project
- Toolchain: **[pioarduino](https://github.com/pioarduino/platform-espressif32)**
  community platform (for a newer Arduino-ESP32 core than PlatformIO's
  official registry currently ships)
