#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

// ESP32 server address
#define SERVER_IP   "192.168.4.1"
#define SERVER_PORT 80

// Save file path on SD card
#define SAVE_FILE "/3ds/rckart.cfg"

// ============================================================
// Settings (saved to SD, synced to ESP32)
// ============================================================

static int cfg_throttle = 75;
static int cfg_steering = 100;
static int cfg_revThrottle = 1;
static int cfg_revSteering = 1;

// ============================================================
// Save / Load
// ============================================================

static void saveConfig(void) {
    FILE* f = fopen(SAVE_FILE, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d\n", cfg_throttle, cfg_steering, cfg_revThrottle, cfg_revSteering);
    fclose(f);
}

static void loadConfig(void) {
    FILE* f = fopen(SAVE_FILE, "r");
    if (!f) return;
    int tp, sp, rt, rs;
    if (fscanf(f, "%d %d %d %d", &tp, &sp, &rt, &rs) == 4) {
        cfg_throttle = tp;
        cfg_steering = sp;
        cfg_revThrottle = rt;
        cfg_revSteering = rs;
    }
    fclose(f);
}

// ============================================================
// Button name mapping
// ============================================================

typedef struct {
    u32 key;
    const char* name;
} ButtonMap;

static const ButtonMap buttons[] = {
    { KEY_A,      "A"  },
    { KEY_B,      "B"  },
    { KEY_X,      "X"  },
    { KEY_Y,      "Y"  },
    { KEY_L,      "L"  },
    { KEY_R,      "R"  },
    { KEY_START,  "ST" },
    { KEY_SELECT, "SE" },
    { KEY_DUP,    "U"  },
    { KEY_DDOWN,  "D"  },
    { KEY_DLEFT,  "LT" },
    { KEY_DRIGHT, "RT" },
};
#define NUM_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

// ============================================================
// Networking
// ============================================================

static int sendHTTPGet(const char* path) {
    struct sockaddr_in server_addr;
    int sock;
    char request[512];
    char response[128];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -2;
    }

    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n",
        path, SERVER_IP);

    send(sock, request, strlen(request), 0);
    recv(sock, response, sizeof(response) - 1, 0);
    close(sock);
    return 0;
}

static void sendButton(const char* id, int pressed) {
    char path[64];
    snprintf(path, sizeof(path), "/b?i=%s&s=%d", id, pressed);
    sendHTTPGet(path);
}

static void sendStick(int x, int y) {
    char path[64];
    snprintf(path, sizeof(path), "/s?x=%d&y=%d", x, y);
    sendHTTPGet(path);
}

static void sendConfig(void) {
    char path[256];
    snprintf(path, sizeof(path),
        "/cfg?tp=%d&sp=%d&rt=%d&rs=%d",
        cfg_throttle, cfg_steering, cfg_revThrottle, cfg_revSteering);
    sendHTTPGet(path);
}

// Sync saved config to ESP32 (called on startup / reconnect)
static int syncConfigToESP(void) {
    return sendHTTPGet("/cfg?tp=0") == 0 ? 0 : -1;  // test connection first
}

static void pushConfigToESP(PrintConsole* topConsole) {
    consoleSelect(topConsole);
    printf("Syncing config to ESP32...\n");

    char path[256];
    snprintf(path, sizeof(path),
        "/cfg?tp=%d&sp=%d&rt=%d&rs=%d",
        cfg_throttle, cfg_steering, cfg_revThrottle, cfg_revSteering);

    if (sendHTTPGet(path) == 0) {
        printf("  Config synced: T=%d%% S=%d%% rT=%d rS=%d\n",
            cfg_throttle, cfg_steering, cfg_revThrottle, cfg_revSteering);
    } else {
        printf("  Sync failed (will retry)\n");
    }
}

// ============================================================
// GUI Drawing (bottom screen console-based)
// ============================================================

static PrintConsole bottomConsole;

static void drawGUI(void) {
    consoleSelect(&bottomConsole);
    consoleClear();

    // Title
    printf("\x1b[1;1H\x1b[33m=== RC KART SETTINGS ===\x1b[0m");

    // Throttle slider
    printf("\x1b[3;1HThrottle Power: %3d%%", cfg_throttle);
    printf("\x1b[4;1H[");
    int tFill = cfg_throttle * 24 / 100;
    for (int i = 0; i < 24; i++) {
        printf("%c", i < tFill ? '#' : '-');
    }
    printf("]");

    // Steering slider
    printf("\x1b[6;1HSteering Power: %3d%%", cfg_steering);
    printf("\x1b[7;1H[");
    int sFill = cfg_steering * 24 / 100;
    for (int i = 0; i < 24; i++) {
        printf("%c", i < sFill ? '#' : '-');
    }
    printf("]");

    // Toggle buttons
    printf("\x1b[9;1HRev Throttle: %s",
        cfg_revThrottle ? "\x1b[32m[ON] \x1b[0m" : "\x1b[31m[OFF]\x1b[0m");
    printf("\x1b[9;22HRev Steering: %s",
        cfg_revSteering ? "\x1b[32m[ON] \x1b[0m" : "\x1b[31m[OFF]\x1b[0m");

    // Drift info
    printf("\x1b[11;1H\x1b[36mDrift: Hold L + A to drift\x1b[0m");
    printf("\x1b[12;1H       Release L for BOOST");

    // Instructions
    printf("\x1b[14;1H\x1b[37mTouch sliders to adjust");
    printf("\x1b[15;1HTouch toggles to flip");
    printf("\x1b[16;1HSettings auto-saved to SD\x1b[0m");

    // Controls reminder
    printf("\x1b[18;1H\x1b[33mControls:\x1b[0m A=Fwd B=Rev");
    printf("\x1b[19;1HStick/DPad=Steer  L+A=Drift");
    printf("\x1b[20;1HSTART+SELECT to exit");
}

// ============================================================
// Touch handling
// ============================================================

static void handleTouch(touchPosition touch) {
    int tx = touch.px;
    int ty = touch.py;
    bool changed = false;

    // Throttle slider area: row 4 (y~24-40), x 8-208
    if (ty >= 24 && ty <= 40 && tx >= 8 && tx <= 208) {
        cfg_throttle = (tx - 8) * 100 / 200;
        if (cfg_throttle < 0) cfg_throttle = 0;
        if (cfg_throttle > 100) cfg_throttle = 100;
        changed = true;
    }

    // Steering slider area: row 7 (y~48-64), x 8-208
    if (ty >= 48 && ty <= 64 && tx >= 8 && tx <= 208) {
        cfg_steering = (tx - 8) * 100 / 200;
        if (cfg_steering < 0) cfg_steering = 0;
        if (cfg_steering > 100) cfg_steering = 100;
        changed = true;
    }

    // Rev Throttle toggle: row 9 (y~64-80), x 8-160
    if (ty >= 64 && ty <= 80 && tx >= 8 && tx <= 160) {
        cfg_revThrottle = !cfg_revThrottle;
        changed = true;
    }

    // Rev Steering toggle: row 9 (y~64-80), x 168-320
    if (ty >= 64 && ty <= 80 && tx >= 168 && tx <= 320) {
        cfg_revSteering = !cfg_revSteering;
        changed = true;
    }

    if (changed) {
        sendConfig();
        saveConfig();
        drawGUI();
    }
}

// ============================================================
// Main
// ============================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    gfxInitDefault();

    // Top screen: status/log console
    PrintConsole topConsole;
    consoleInit(GFX_TOP, &topConsole);

    // Bottom screen: settings GUI
    consoleInit(GFX_BOTTOM, &bottomConsole);

    consoleSelect(&topConsole);
    printf("=== RC Kart Controller ===\n\n");
    printf("Server: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Connect to '3DS_Controller' WiFi\n");
    printf("Password: 12345678\n\n");

    // Load saved preferences from SD
    loadConfig();
    printf("Loaded config: T=%d%% S=%d%% rT=%d rS=%d\n",
        cfg_throttle, cfg_steering, cfg_revThrottle, cfg_revSteering);

    printf("Press START+SELECT to exit\n");
    printf("---\n\n");

    // Initialize networking
    u32 *SOC_buffer = (u32*)memalign(0x1000, 0x100000);
    if (SOC_buffer == NULL) {
        printf("Failed to allocate SOC buffer!\n");
        goto exit_fail;
    }

    Result ret = socInit(SOC_buffer, 0x100000);
    if (R_FAILED(ret)) {
        printf("socInit failed: 0x%08lX\n", ret);
        free(SOC_buffer);
        goto exit_fail;
    }

    printf("Network initialized!\n");

    // Push saved config to ESP32 on startup
    // Retry a few times since WiFi might not be ready yet
    {
        int synced = 0;
        for (int attempt = 0; attempt < 5; attempt++) {
            printf("Connecting to ESP32 (attempt %d)...\n", attempt + 1);
            char path[256];
            snprintf(path, sizeof(path),
                "/cfg?tp=%d&sp=%d&rt=%d&rs=%d",
                cfg_throttle, cfg_steering, cfg_revThrottle, cfg_revSteering);
            if (sendHTTPGet(path) == 0) {
                printf("Config synced to ESP32!\n");
                synced = 1;
                break;
            }
            // Wait a bit before retrying
            svcSleepThread(1000000000LL);  // 1 second
        }
        if (!synced) {
            printf("Could not sync yet. Will send on first input.\n");
        }
    }

    printf("\nSending inputs to ESP32...\n\n");

    // Draw initial GUI
    drawGUI();
    consoleSelect(&topConsole);

    int prevStickX = 0, prevStickY = 0;
    u32 prevHeld = 0;
    (void)prevHeld;
    bool touchHeld = false;
    bool configSentThisSession = false;
    (void)configSentThisSession;

    // Main loop
    while (aptMainLoop()) {
        hidScanInput();
        u32 held = hidKeysHeld();
        u32 down = hidKeysDown();
        u32 up = hidKeysUp();

        // Exit on START+SELECT
        if ((held & KEY_START) && (held & KEY_SELECT)) break;

        // --- Touch screen handling ---
        if (held & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (!touchHeld) {
                handleTouch(touch);
                touchHeld = true;
            } else {
                // Dragging on sliders
                int tx = touch.px;
                int ty = touch.py;
                bool sliderDrag = false;

                if (ty >= 24 && ty <= 40 && tx >= 8 && tx <= 208) {
                    cfg_throttle = (tx - 8) * 100 / 200;
                    if (cfg_throttle < 0) cfg_throttle = 0;
                    if (cfg_throttle > 100) cfg_throttle = 100;
                    sliderDrag = true;
                }
                if (ty >= 48 && ty <= 64 && tx >= 8 && tx <= 208) {
                    cfg_steering = (tx - 8) * 100 / 200;
                    if (cfg_steering < 0) cfg_steering = 0;
                    if (cfg_steering > 100) cfg_steering = 100;
                    sliderDrag = true;
                }
                if (sliderDrag) {
                    drawGUI();
                }
            }
        } else {
            if (touchHeld) {
                // Touch released - send final config and save
                sendConfig();
                saveConfig();
                touchHeld = false;
            }
        }

        // --- Circle pad ---
        circlePosition pos;
        hidCircleRead(&pos);

        int stickX = (pos.dx * 100) / 156;
        int stickY = (pos.dy * 100) / 156;
        if (stickX > 100) stickX = 100;
        if (stickX < -100) stickX = -100;
        if (stickY > 100) stickY = 100;
        if (stickY < -100) stickY = -100;

        // Dead zone
        if (abs(stickX) < 10) stickX = 0;
        if (abs(stickY) < 10) stickY = 0;

        // --- Send button events ---
        consoleSelect(&topConsole);
        for (int i = 0; i < (int)NUM_BUTTONS; i++) {
            if (down & buttons[i].key) {
                printf("  [PRESS]   %s\n", buttons[i].name);
                sendButton(buttons[i].name, 1);
            }
            if (up & buttons[i].key) {
                printf("  [RELEASE] %s\n", buttons[i].name);
                sendButton(buttons[i].name, 0);
            }
        }

        // Send stick only when changed
        if (stickX != prevStickX || stickY != prevStickY) {
            printf("  [STICK] x=%d y=%d\n", stickX, stickY);
            sendStick(stickX, stickY);
            prevStickX = stickX;
            prevStickY = stickY;
        }

        prevHeld = held;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    // Save config one final time on exit
    saveConfig();

    printf("\nShutting down...\n");
    socExit();
    free(SOC_buffer);

exit_fail:
    consoleSelect(&topConsole);
    printf("\nPress A to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_A) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
