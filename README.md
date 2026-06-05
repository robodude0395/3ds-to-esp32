# 3DS ESP32 Controller

> Today I woke up in a cold sweat with a chilling realization. Of all the inefficient and absurd ways to control a robot… what if I used a literal Nintendo 3DS? And so, possessed by this cursed vision, I speedran vibe-coding this into existence. For the memes.

A system that turns a Nintendo 3DS into a wireless controller for an ESP32. The 3DS reads physical button and circle pad inputs via a homebrew app and sends them over WiFi to an ESP32 running as an access point.

## How It Works

```
┌─────────────┐   WiFi (HTTP/TCP)   ┌─────────────┐
│  Nintendo   │ ──────────────────► │    ESP32     │
│     3DS     │  button/stick data  │   (AP mode)  │
│  homebrew   │ ◄────────────────── │  web server  │
│    app      │      responses      │              │
└─────────────┘                     └──────────────┘
                                          │
                                          ▼
                                    GPIO outputs
                                    Serial monitor
                                    Motor control
                                    Whatever you want
```

- **ESP32** creates a WiFi access point (`3DS_Controller`) and listens for HTTP requests
- **3DS homebrew app** connects to that network and sends button/stick state as HTTP GET requests
- The ESP32 parses inputs and exposes them via the `N3DSController` library for easy integration into any project

## Protocol

The 3DS communicates using **HTTP/1.1 over TCP** (port 80). Each input event is a GET request:

| Input | Request | Values |
|-------|---------|--------|
| Button press/release | `GET /b?i=<ID>&s=<0\|1>` | s=1 press, s=0 release |
| Circle pad movement | `GET /s?x=<X>&y=<Y>` | -100 to 100 per axis |

Button IDs: `A`, `B`, `X`, `Y`, `L`, `R`, `ST` (Start), `SE` (Select), `U`, `D`, `LT` (Left), `RT` (Right)

Each request opens a TCP connection, sends headers, gets a `200 OK`, and closes. Simple and reliable at human input speeds.

## Project Structure

```
├── src/main.cpp              # ESP32 firmware example (PlatformIO/Arduino)
├── platformio.ini            # PlatformIO config (espressif32, esp32dev)
├── lib/N3DSController/       # Reusable Arduino library
│   ├── N3DSController.h      # Header with N3DSInput struct + API
│   └── N3DSController.cpp    # Implementation
├── 3ds_client/               # Nintendo 3DS homebrew app
│   ├── source/main.c         # Reads inputs, sends HTTP to ESP32
│   ├── Makefile              # devkitARM build (outputs .3dsx and .cia)
│   ├── app.rsf               # CIA metadata for Home Menu install
│   └── icon.png              # App icon (48x48 PNG, provide your own)
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

Build the .3dsx (Homebrew Launcher):

```bash
cd 3ds_client
make
```

Build the .cia (Home Menu install):

```bash
make cia
```

Requires `cxitool` and `makerom` in your PATH:

```bash
# Install cxitool (from devkitPro 3dstools)
brew install autoconf automake libtool  # if not already installed
git clone -b cxi-stuff https://github.com/devkitPro/3dstools.git /tmp/3dstools
cd /tmp/3dstools
autoreconf -fi
./configure
make
sudo cp cxitool /usr/local/bin/

# Install makerom
git clone https://github.com/3DSGuy/Project_CTR.git /tmp/Project_CTR
cd /tmp/Project_CTR/makerom
make deps
make
sudo cp bin/makerom /usr/local/bin/
```

### 3. Install on 3DS

**Option A — Homebrew Launcher (.3dsx):**
1. Copy `3ds_client.3dsx` to `/3ds/` on your SD card
2. Open Homebrew Launcher and run it

**Option B — Home Menu (.cia):**
1. Copy `3ds_client.cia` to your SD card
2. Open FBI → navigate to the file → Install
3. App appears on Home Menu with your custom icon

### 4. Connect

1. On the 3DS: **System Settings → Internet Settings → Connection Settings**
2. Add `3DS_Controller` network (password: `12345678`)
3. Set DNS to **Manual**: Primary `192.168.4.1`, Secondary `192.168.4.1`
4. Save and run connection test (should pass)
5. Launch the app
6. Press buttons / move circle pad — inputs show in ESP32 Serial Monitor

Press **START + SELECT** together to exit the app.

## Using the Library

The `N3DSController` library lives in `lib/` and can be dropped into any PlatformIO project.

```cpp
#include <N3DSController.h>

N3DSController controller;

void setup() {
  controller.begin();
}

void loop() {
  controller.update();

  const N3DSInput& input = controller.getInput();

  // Use input.A, input.B, input.stickX, input.stickY, etc.
  // Example: differential drive
  int leftMotor  = input.stickY + input.stickX;
  int rightMotor = input.stickY - input.stickX;
}
```

### Available Inputs

| Field | Type | Description |
|-------|------|-------------|
| `input.A` | bool | A button (right face) |
| `input.B` | bool | B button (bottom face) |
| `input.X` | bool | X button (top face) |
| `input.Y` | bool | Y button (left face) |
| `input.L` | bool | Left shoulder |
| `input.R` | bool | Right shoulder |
| `input.Start` | bool | Start button |
| `input.Select` | bool | Select button |
| `input.Up` | bool | D-pad up |
| `input.Down` | bool | D-pad down |
| `input.Left` | bool | D-pad left |
| `input.Right` | bool | D-pad right |
| `input.stickX` | int | Circle pad X (-100 left, +100 right) |
| `input.stickY` | int | Circle pad Y (-100 down, +100 up) |

Helpers:
- `input.anyActive()` — true if anything is pressed or stick is off-center
- `input.clear()` — reset to neutral

### API

| Method | Description |
|--------|-------------|
| `controller.begin()` | Start AP + server |
| `controller.update()` | Process requests (call every loop) |
| `controller.getInput()` | Get current input state |
| `controller.isConnected()` | True if 3DS sent data recently |
| `controller.onInput(callback)` | Called on every input change |
| `controller.setAP(ssid, pass)` | Custom AP credentials (before begin) |
| `controller.setInputTimeout(ms)` | Auto-clear delay, 0 = never (default: 2000) |

## Customizing

**Change AP name/password:**
```cpp
controller.setAP("MyRobot", "secretpass");
controller.begin();
```

**React to input changes with a callback:**
```cpp
void onInput(const N3DSInput& input) {
  if (input.A) fire();
  if (input.L && input.R) turbo();
}
controller.onInput(onInput);
```

**To reuse in another project:** copy the `lib/N3DSController/` folder into your new project's `lib/` directory. PlatformIO picks it up automatically.

## DNS / Connectivity Check

The ESP32 runs a DNS server that resolves all domains to itself and spoofs Nintendo's connectivity check (`conntest.nintendowifi.net`). Setting DNS to `192.168.4.1` on the 3DS makes the connection test pass without real internet.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| 3DS connection test fails | Set DNS manually to `192.168.4.1` in connection settings |
| Stock browser says "no internet" | Use the homebrew app — stock browser's check is separate and stubborn |
| `intelhex` error building ESP32 | `~/.platformio/penv/bin/pip install intelhex` |
| 3DS app build fails: `close` | Ensure `#include <unistd.h>` is in main.c |
| No inputs in serial monitor | Verify 3DS is on `3DS_Controller` WiFi before launching app |
| ESP32 upload: "wrong chip" | Use `esp32dev` board in platformio.ini (not esp8266) |
| CIA won't build | Install `cxitool` and `makerom`, ensure they're in PATH |
| App needs icon | Place a 48x48 PNG as `3ds_client/icon.png` |
| `autoreconf` not found | `brew install autoconf automake libtool` |
