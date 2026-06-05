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

// How often to send updates (ms)
#define SEND_INTERVAL 50

// Button name mapping
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

// Send a quick HTTP GET request (fire and forget)
static int sendHTTPGet(const char* path) {
    struct sockaddr_in server_addr;
    int sock;
    char request[256];
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

    // Read just enough to complete the request
    recv(sock, response, sizeof(response) - 1, 0);

    close(sock);
    return 0;
}

// Send button state
static void sendButton(const char* id, int pressed) {
    char path[64];
    snprintf(path, sizeof(path), "/b?i=%s&s=%d", id, pressed);
    sendHTTPGet(path);
}

// Send stick position
static void sendStick(int x, int y) {
    char path[64];
    snprintf(path, sizeof(path), "/s?x=%d&y=%d", x, y);
    sendHTTPGet(path);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("=== 3DS Controller Client ===\n\n");
    printf("Server: %s:%d\n\n", SERVER_IP, SERVER_PORT);
    printf("Connect to '3DS_Controller' WiFi\n");
    printf("Password: 12345678\n\n");
    printf("Press START+SELECT to exit\n\n");
    printf("---\n\n");

    // Initialize SOC service for networking
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
    printf("Sending inputs to ESP32...\n\n");

    u32 prevHeld = 0;
    int prevStickX = 0, prevStickY = 0;
    bool connected = false;
    (void)connected;
    (void)prevHeld;

    // Main loop
    while (aptMainLoop()) {
        hidScanInput();
        u32 held = hidKeysHeld();
        u32 down = hidKeysDown();
        u32 up = hidKeysUp();

        // Exit on START+SELECT
        if ((held & KEY_START) && (held & KEY_SELECT)) break;

        // Circle pad
        circlePosition pos;
        hidCircleRead(&pos);

        // Normalize circle pad to -100..100
        int stickX = (pos.dx * 100) / 156;
        int stickY = (pos.dy * 100) / 156;
        if (stickX > 100) stickX = 100;
        if (stickX < -100) stickX = -100;
        if (stickY > 100) stickY = 100;
        if (stickY < -100) stickY = -100;

        // Dead zone
        if (abs(stickX) < 10) stickX = 0;
        if (abs(stickY) < 10) stickY = 0;

        // Send button presses
        for (int i = 0; i < (int)NUM_BUTTONS; i++) {
            if (down & buttons[i].key) {
                printf("  [PRESS]   %s\n", buttons[i].name);
                sendButton(buttons[i].name, 1);
                connected = true;
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

        // Render
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    printf("\nShutting down...\n");
    socExit();
    free(SOC_buffer);

exit_fail:
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
