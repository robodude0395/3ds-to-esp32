# 3DS ESP32 Controller

> Today I woke up in a cold sweat with a chilling realization. Of all the inefficient and absurd ways to control a robot… what if I used a literal Nintendo 3DS? And so, possessed by this cursed vision, I speedran vibe-coding this into existence. For the memes.

A system that turns a Nintendo 3DS into a wireless controller for an ESP32. The 3DS reads physical buttons and circle pad via a homebrew app and sends them over WiFi to an ESP32 running as an access point.

## How It Works

```
┌─────────────┐   WiFi (HTTP/TCP)   ┌──────────────┐
│  Nintendo   │ ──────────────────► │    ESP32      │
│     3DS     │  button/stick data  │   (AP mode)   │
│  homebrew   │ ◄────────────────── │  web server   │
│    app      │      responses      │               │
└─────────────┘                     └───────────────┘
                                          │
                                          ▼
                                    GPIO outputs
                                    Motor control
                                    Whatever you want
```

## Project Structure

```
├── src/main.cpp              # ESP32 example firmware
├── platformio.ini            # PlatformIO config (esp32dev)
├── lib/N3DSController/       # Reusable Arduino library
│   ├── N3DSController.h      # API + N3DSInput struct
│   └── N3DSController.cpp    # Implementation
└── 3ds_client/               # Nintendo 3DS homebrew app
    ├── source/main.c         # Reads inputs, sends HTTP to ESP32
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

Open Serial Monitor at 115200 baud to see incoming inputs.

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

**Build .cia** (for Home Menu — generic icon, but app works):

Requires `cxitool` and `makerom`:

```bash
# Install cxitool
brew install autoconf automake libtool
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

Then:

```bash
cd 3ds_client
make cia
```

### 3. Install on 3DS

**Option A — Homebrew Launcher (.3dsx) — recommended:**

Copy `3ds_client.3dsx` to `/3ds/3ds_client/3ds_client.3dsx` on your SD card. Launch via Homebrew Launcher. This shows your custom icon, title, and author.

**Option B — Home Menu (.cia):**

```bash
make cia
```

Copy `3ds_client.cia` to SD card. Open FBI → navigate to file → Install. The app will work but shows a generic icon on the Home Menu (a limitation of the cxitool conversion method).

### 4. Connect

1. On 3DS: **System Settings → Internet Settings → Connection Settings**
2. Add `3DS_Controller` (password: `12345678`)
3. Set DNS to **Manual**: Primary and Secondary both `192.168.4.1`
4. Save and run connection test (should pass)
5. Launch the app
6. Press buttons / move circle pad — inputs show in ESP32 Serial Monitor

Press **START + SELECT** to exit the app.

## Using the Library

Copy `lib/N3DSController/` into any PlatformIO project's `lib/` folder.

```cpp
#include <N3DSController.h>

N3DSController controller;

void setup() {
  controller.begin();
}

void loop() {
  controller.update();

  const N3DSInput& input = controller.getInput();

  // Differential drive example
  int leftMotor  = input.stickY + input.stickX;
  int rightMotor = input.stickY - input.stickX;
}
```

### Available Inputs

| Field | Type | Description |
|-------|------|-------------|
| `input.A` | bool | A button |
| `input.B` | bool | B button |
| `input.X` | bool | X button |
| `input.Y` | bool | Y button |
| `input.L` | bool | Left shoulder |
| `input.R` | bool | Right shoulder |
| `input.Start` | bool | Start |
| `input.Select` | bool | Select |
| `input.Up` | bool | D-pad up |
| `input.Down` | bool | D-pad down |
| `input.Left` | bool | D-pad left |
| `input.Right` | bool | D-pad right |
| `input.stickX` | int | Circle pad X (-100 to 100) |
| `input.stickY` | int | Circle pad Y (-100 to 100) |

Helpers: `input.anyActive()`, `input.clear()`

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

## Protocol

HTTP/1.1 GET requests over TCP port 80:

| Input | Request |
|-------|---------|
| Button | `GET /b?i=<ID>&s=<0\|1>` |
| Stick | `GET /s?x=<X>&y=<Y>` |

Button IDs: `A`, `B`, `X`, `Y`, `L`, `R`, `ST`, `SE`, `U`, `D`, `LT`, `RT`

## Customizing the 3DS App

**Icon:** Replace `3ds_client/icon.png` (must be 48×48 PNG) — shows in Homebrew Launcher

**Author/title:** Edit `APP_TITLE`, `APP_DESCRIPTION`, `APP_AUTHOR` in `3ds_client/Makefile`

## DNS / Connectivity Check

The ESP32 runs a DNS server that resolves all domains to itself and spoofs Nintendo's `conntest.nintendowifi.net` response. Setting DNS to `192.168.4.1` on the 3DS makes the connection test pass without real internet.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| 3DS connection test fails | Set DNS manually to `192.168.4.1` |
| Stock browser says no internet | Use the homebrew app instead |
| `intelhex` error building ESP32 | `~/.platformio/penv/bin/pip install intelhex` |
| 3DS build: `close` undefined | Ensure `#include <unistd.h>` in main.c |
| No inputs in serial | Verify 3DS is on `3DS_Controller` WiFi first |
| ESP32 upload: wrong chip | Use `esp32dev` board in platformio.ini |
| CIA build fails | Install `cxitool` and `makerom` per instructions above |
| `autoreconf` not found | `brew install autoconf automake libtool` |
| CIA shows generic icon | Normal — use .3dsx via Homebrew Launcher for custom icon |
