# 3DS ESP32 RC Kart Controller

> Today I woke up in a cold sweat with a chilling realization. Of all the inefficient and absurd ways to control a robot… what if I used a literal Nintendo 3DS? And so, possessed by this cursed vision, I speedran vibe-coding this into existence. For the memes.

A system that turns a Nintendo 3DS into a wireless RC car controller via an ESP32. The 3DS reads physical buttons and circle pad input through a homebrew app with a touchscreen settings GUI and sends them over WiFi to an ESP32 driving servos for throttle and steering. Mario Kart-style drift mode included.

## How It Works

```
┌─────────────────┐  WiFi (HTTP)  ┌──────────────────┐
│   Nintendo 3DS  │ ────────────► │      ESP32       │
│                 │  batched      │    (AP mode)     │
│  Top: log/HUD  │  input state  │                  │
│  Bot: settings  │               │  D4  → Throttle │
│       GUI       │ ◄──────────── │  D16 → Steering │
│                 │   responses   │                  │
└─────────────────┘               └──────────────────┘
```

## Features

- **Servo-based RC control** — D4 for throttle (ESC), D16 for steering servo
- **Batched input protocol** — all buttons + stick sent in a single HTTP request per frame, no input blocking
- **Throttle ramping** — gradual acceleration to prevent power supply brown-outs
- **Drift mode** — hold L + A to pulse throttle between low/high, release L for speed boost
- **Touchscreen settings GUI** — adjust throttle %, steering %, and reverse flags on the 3DS bottom screen
- **Persistent config** — settings saved to SD card and auto-synced to ESP32 on startup
- **Configurable via HTTP** — all parameters tunable at runtime through `/cfg` endpoint

## Project Structure

```
├── src/main.cpp              # ESP32 RC kart firmware
├── platformio.ini            # PlatformIO config + ESP32Servo dep
├── lib/N3DSController/       # Reusable Arduino library
│   ├── N3DSController.h      # API + N3DSInput struct
│   └── N3DSController.cpp    # Web server + batched input handler
└── 3ds_client/               # Nintendo 3DS homebrew app
    ├── source/main.c         # Input handling, GUI, save/load
    ├── Makefile              # Builds .3dsx and .cia
    └── icon.png              # 48x48 app icon
```

## Quick Start

### 1. Flash the ESP32

Open this project in PlatformIO (VSCode) and upload:

```bash
pio run --target upload
```

The ESP32 creates a WiFi AP:
- **SSID:** `3DS_Controller`
- **Password:** `12345678`
- **IP:** `192.168.4.1`

### 2. Build the 3DS App

**Prerequisites:**

```bash
# Install devkitPro (macOS)
curl -L https://github.com/devkitPro/pacman/releases/latest/download/devkitpro-pacman-installer.pkg -o /tmp/devkitpro.pkg
sudo installer -pkg /tmp/devkitpro.pkg -target /

# Install 3DS toolchain
sudo dkp-pacman -Sy
sudo dkp-pacman -S 3ds-dev

# Add to ~/.zshrc (then source ~/.zshrc)
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitARM/bin:$PATH
```

**Build .3dsx** (for Homebrew Launcher):

```bash
cd 3ds_client
make
```

**Build .cia** (for Home Menu):

Requires `cxitool` and `makerom`:

```bash
# Install cxitool
brew install autoconf automake libtool
git clone -b cxi-stuff https://github.com/devkitPro/3dstools.git /tmp/3dstools
cd /tmp/3dstools && autoreconf -fi && ./configure && make
sudo cp cxitool /usr/local/bin/

# Install makerom
git clone https://github.com/3DSGuy/Project_CTR.git /tmp/Project_CTR
cd /tmp/Project_CTR/makerom && make deps && make
sudo cp bin/makerom /usr/local/bin/
```

Then:

```bash
cd 3ds_client
make cia
```

### 3. Install on 3DS

**Option A — Homebrew Launcher (.3dsx) — recommended:**

Copy `3ds_client.3dsx` to `/3ds/3ds_client/3ds_client.3dsx` on your SD card.

**Option B — Home Menu (.cia):**

Copy `3ds_client.cia` to SD card. Open FBI → Install. Shows generic icon on Home Menu.

### 4. Connect

1. On 3DS: **System Settings → Internet Settings → Connection Settings**
2. Add `3DS_Controller` (password: `12345678`)
3. Set DNS to **Manual**: Primary and Secondary both `192.168.4.1`
4. Save and run connection test (should pass)
5. Launch the app — config auto-syncs to ESP32

## Controls

| Input | Action |
|-------|--------|
| A | Forward (at configured throttle %) |
| B | Reverse (at configured throttle %) |
| Circle Pad (horizontal) | Proportional steering |
| D-pad Left / Right | Full steering lock |
| L + A (hold) | Drift mode — throttle pulses to maintain slide |
| Release L (during drift) | Boost for configurable duration |
| Touch bottom screen | Adjust settings (sliders, toggles) |
| START + SELECT | Exit app |

## Touchscreen Settings GUI

The 3DS bottom screen provides a live settings interface:

- **Throttle Power** — slider (0-100%) controls max forward/reverse power
- **Steering Power** — slider (0-100%) controls max steering throw
- **Rev Throttle** — toggle to reverse throttle direction
- **Rev Steering** — toggle to reverse steering direction

Settings are saved to `/3ds/rckart.cfg` on the SD card and automatically pushed to the ESP32 on app startup.

## ESP32 Tunable Parameters

All parameters can be adjusted at runtime via `GET /cfg?param=value`:

| Parameter | Key | Default | Description |
|-----------|-----|---------|-------------|
| Throttle % | `tp` | 75 | Max throttle power (0-100) |
| Steering % | `sp` | 100 | Max steering throw (0-100) |
| Reverse Throttle | `rt` | 1 | Flip throttle direction |
| Reverse Steering | `rs` | 1 | Flip steering direction |
| Ramp Rate | `rr` | 15 | Throttle ramp speed (µs/tick, 1-500) |
| Drift Low | `dl` | 50 | Drift pulse low throttle % |
| Drift High | `dh` | 95 | Drift pulse high throttle % |
| Drift Pulse High | `dph` | 150 | Time at high throttle (ms) |
| Drift Pulse Low | `dpl` | 100 | Time at low throttle (ms) |
| Boost % | `bp` | 100 | Boost throttle power |
| Boost Duration | `bd` | 500 | Boost time after drift (ms) |

Read current config: `GET /cfg_get`

## Throttle Ramping

The ESP32 gradually ramps throttle output to prevent sudden current spikes that can brown-out the power supply. The `throttleRampRate` parameter controls how many microseconds the servo signal can change per loop tick (~50Hz). Lower values = gentler acceleration:

- `5` — very gentle (~5s to full throttle)
- `15` — default (~1.5s to full throttle)
- `25` — snappy (~1s)
- `50` — fast (~0.5s, test your PSU)

## Protocol

The 3DS sends a single batched HTTP request per frame containing all input state:

```
GET /i?b=<hex_bitmask>&x=<stickX>&y=<stickY>
```

**Button bitmask** (bit 0 = LSB):

| Bit | Button | Bit | Button |
|-----|--------|-----|--------|
| 0 | A | 6 | Start |
| 1 | B | 7 | Select |
| 2 | X | 8 | D-Up |
| 3 | Y | 9 | D-Down |
| 4 | L | 10 | D-Left |
| 5 | R | 11 | D-Right |

Legacy individual endpoints still supported for backward compatibility:

| Input | Request |
|-------|---------|
| Button | `GET /b?i=<ID>&s=<0\|1>` |
| Stick | `GET /s?x=<X>&y=<Y>` |

## Using the Library

Copy `lib/N3DSController/` into any PlatformIO project's `lib/` folder.

```cpp
#include <N3DSController.h>

N3DSController controller;

void setup() {
  controller.begin();

  // Add custom endpoints
  controller.addEndpoint("/my_endpoint", HTTP_GET, []() {
    controller.getServer().send(200, "text/plain", "hello");
  });
}

void loop() {
  controller.update();
  const N3DSInput& input = controller.getInput();

  // Use inputs however you want
  if (input.A) { /* ... */ }
  int steering = input.stickX;  // -100 to 100
}
```

### API

| Method | Description |
|--------|-------------|
| `controller.begin()` | Start AP + server |
| `controller.update()` | Process requests (call every loop) |
| `controller.getInput()` | Get current input state |
| `controller.isConnected()` | True if 3DS sent data recently |
| `controller.onInput(cb)` | Callback on input change |
| `controller.setAP(ssid, pass)` | Custom AP (call before begin) |
| `controller.setInputTimeout(ms)` | Auto-clear delay (default: 2000) |
| `controller.getServer()` | Access internal WebServer |
| `controller.addEndpoint(uri, method, handler)` | Register custom HTTP routes |

## Pin Configuration

The firmware uses two of the 12 available PWM outputs:

| Pin | Function | Signal |
|-----|----------|--------|
| D4 | Throttle / ESC | 1000-2000µs (1500 = neutral) |
| D16 | Steering servo | 1000-2000µs (1500 = center) |

Available PWM pins on the board: D4, D16, D17, D18, D19, D25, D13, D14, D26, D27, D32, D33

## DNS / Connectivity Check

The ESP32 runs a DNS server that resolves all domains to itself and spoofs Nintendo's `conntest.nintendowifi.net` response. Setting DNS to `192.168.4.1` on the 3DS makes the connection test pass without real internet.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| 3DS connection test fails | Set DNS manually to `192.168.4.1` |
| ESP32 resets on throttle | Lower `throttleRampRate` (try 5-10) |
| Steering doesn't respond while driving | Update 3DS app — old version sends individual blocking requests |
| Controls are reversed | Toggle reverse flags via touchscreen GUI |
| Settings lost on restart | Check SD card has `/3ds/` folder writable |
| `intelhex` error building ESP32 | `~/.platformio/penv/bin/pip install intelhex` |
| 3DS build: `close` undefined | Ensure `#include <unistd.h>` in main.c |
| No inputs in serial | Verify 3DS is on `3DS_Controller` WiFi |
| CIA shows generic icon | Normal — use .3dsx via Homebrew Launcher for custom icon |
