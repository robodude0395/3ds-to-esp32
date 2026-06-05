# ESP32 + Nintendo 3DS Controller

A system that turns a Nintendo 3DS into a wireless controller for an ESP32. The 3DS reads physical button and circle pad inputs and sends them over WiFi to an ESP32 running as an access point.

## How It Works

```
┌─────────────┐     WiFi (HTTP)     ┌─────────────┐
│  Nintendo   │ ──────────────────► │    ESP32     │
│     3DS     │  button/stick data  │   (AP mode)  │
│  homebrew   │ ◄────────────────── │  web server  │
│    app      │      responses      │             │
└─────────────┘                     └─────────────┘
                                          │
                                          ▼
                                    GPIO outputs
                                    Serial monitor
```

- **ESP32** creates a WiFi network called `3DS_Controller` and runs an HTTP server
- **3DS homebrew app** connects to that network and sends button/stick state as HTTP requests
- The ESP32 parses inputs and can drive GPIO pins, log to serial, etc.

## Project Structure

```
├── src/main.cpp          # ESP32 firmware (PlatformIO/Arduino)
├── platformio.ini        # PlatformIO build config (espressif32, esp32dev)
├── 3ds_client/           # Nintendo 3DS homebrew app
│   ├── source/main.c    # 3DS app source (reads inputs, sends HTTP)
│   └── Makefile          # devkitARM build system
└── README.md
```

## Quick Start

### 1. Flash the ESP32

Open this project in PlatformIO (VSCode) and upload:

```bash
pio run --target upload
```

Once flashed, the ESP32 creates a WiFi AP:
- **SSID:** `3DS_Controller`
- **Password:** `12345678`
- **IP:** `192.168.4.1`

Open Serial Monitor at 115200 baud to see incoming inputs.

### 2. Build the 3DS App

**Prerequisites:** [devkitPro](https://devkitpro.org/wiki/Getting_Started) with 3DS support.

```bash
# Install devkitPro (macOS)
curl -L https://github.com/devkitPro/pacman/releases/latest/download/devkitpro-pacman-installer.pkg -o /tmp/devkitpro.pkg
sudo installer -pkg /tmp/devkitpro.pkg -target /

# Install 3DS toolchain
sudo dkp-pacman -Sy
sudo dkp-pacman -S 3ds-dev

# Add to ~/.zshrc (then `source ~/.zshrc`)
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitARM/bin:$PATH
```

Build:

```bash
cd 3ds_client
make
```

Output: `3ds_client.3dsx`

### 3. Install on 3DS

1. Copy `3ds_client.3dsx` to the `/3ds/` folder on your 3DS SD card
2. Launch via **Homebrew Launcher** (Rosalina menu → Miscellaneous → Switch to hb:title, then open Download Play)

### 4. Connect

1. On the 3DS, go to **System Settings → Internet Settings → Connection Settings**
2. Add `3DS_Controller` network (password: `12345678`)
3. Set DNS to **Manual**: Primary `192.168.4.1`, Secondary `192.168.4.1`
4. Save and run connection test (should pass)
5. Launch the homebrew app from Homebrew Launcher
6. Press buttons / move circle pad — inputs appear in ESP32 Serial Monitor

Press **START + SELECT** together to exit the 3DS app.

## Inputs Sent

| Input | HTTP Request | Values |
|-------|-------------|--------|
| Buttons (A/B/X/Y/L/R/Start/Select/D-pad) | `GET /b?i=<ID>&s=<0\|1>` | s=1 press, s=0 release |
| Circle Pad | `GET /s?x=<X>&y=<Y>` | -100 to 100, with dead zone |

Button IDs: `A`, `B`, `X`, `Y`, `L`, `R`, `ST`, `SE`, `U`, `D`, `LT`, `RT`

## ESP32 Outputs

By default, only the built-in LED (GPIO 2) reacts to inputs. To wire up actual outputs, edit `src/main.cpp`:

- In `setup()`: uncomment `pinMode(...)` lines for your pins
- In the `loop()`: add `digitalWrite()` or `analogWrite()` calls based on `state`

The struct `state` holds all current button/stick values and is accessible anywhere.

## Web Interface (Fallback)

The ESP32 also serves a browser-based controller at `http://192.168.4.1/`. This uses an iframe trick to work without JavaScript (compatible with NetSurf 3DS). The homebrew app is preferred since it supports real analog input and physical buttons.

## DNS / Connectivity Check

The ESP32 runs a DNS server that resolves all domains to itself and spoofs Nintendo's connectivity check response (`conntest.nintendowifi.net`). This is why setting the DNS to `192.168.4.1` on the 3DS makes the connection test pass — the 3DS thinks it has internet.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| 3DS connection test fails | Set DNS manually to `192.168.4.1` in the connection settings |
| Stock browser says "no internet" | Use the homebrew app instead; stock browser is stubborn |
| `intelhex` error when building ESP32 | Run `~/.platformio/penv/bin/pip install intelhex` |
| 3DS app build fails with `close` error | Make sure `#include <unistd.h>` is in main.c |
| No inputs showing in serial | Verify 3DS is connected to `3DS_Controller` WiFi before launching app |
| ESP32 upload says "wrong chip" | Use `esp32dev` board in platformio.ini (not esp8266) |
