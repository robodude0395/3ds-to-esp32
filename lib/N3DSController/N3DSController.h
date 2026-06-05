#ifndef N3DS_CONTROLLER_H
#define N3DS_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

// Controller state struct - read this from your main code
//
// === Available Inputs ===
//
// Buttons (bool, true = pressed, false = released):
//   input.A       - A button (right face button)
//   input.B       - B button (bottom face button)
//   input.X       - X button (top face button)
//   input.Y       - Y button (left face button)
//   input.L       - Left shoulder button
//   input.R       - Right shoulder button
//   input.Start   - Start button
//   input.Select  - Select button
//   input.Up      - D-pad up
//   input.Down    - D-pad down
//   input.Left    - D-pad left
//   input.Right   - D-pad right
//
// Analog (int, range -100 to 100):
//   input.stickX  - Circle pad horizontal (-100 = full left, 100 = full right)
//   input.stickY  - Circle pad vertical (-100 = full down, 100 = full up)
//                   Note: 10% dead zone applied on 3DS side
//
// Helpers:
//   input.anyActive()  - Returns true if any button pressed or stick off-center
//   input.clear()      - Resets all inputs to neutral
//
struct N3DSInput {
  // Buttons (true = pressed)
  bool A = false;
  bool B = false;
  bool X = false;
  bool Y = false;
  bool L = false;
  bool R = false;
  bool Start = false;
  bool Select = false;
  bool Up = false;
  bool Down = false;
  bool Left = false;
  bool Right = false;

  // Circle pad (-100 to 100)
  int stickX = 0;
  int stickY = 0;

  // Returns true if any button is pressed or stick is off-center
  bool anyActive() const {
    return A || B || X || Y || L || R || Start || Select ||
           Up || Down || Left || Right || stickX != 0 || stickY != 0;
  }

  // Reset everything to neutral
  void clear() {
    A = B = X = Y = L = R = Start = Select = false;
    Up = Down = Left = Right = false;
    stickX = stickY = 0;
  }
};

// Callback type for when input is received
typedef void (*N3DSInputCallback)(const N3DSInput& input);

class N3DSController {
public:
  // Configure AP credentials (call before begin)
  void setAP(const char* ssid, const char* password);

  // Start the controller server
  void begin();

  // Call this in loop() - handles WiFi clients and DNS
  void update();

  // Get current input state
  const N3DSInput& getInput() const;

  // Check if a 3DS client is connected (received input recently)
  bool isConnected(unsigned long timeoutMs = 3000) const;

  // Set a callback for when new input arrives
  void onInput(N3DSInputCallback callback);

  // Auto-clear inputs after timeout (0 = disabled)
  void setInputTimeout(unsigned long ms);

private:
  const char* _ssid = "3DS_Controller";
  const char* _pass = "12345678";
  DNSServer _dns;
  WebServer _server{80};
  N3DSInput _input;
  N3DSInputCallback _callback = nullptr;
  unsigned long _lastInput = 0;
  unsigned long _timeout = 2000;

  void _handleButton();
  void _handleStick();
  void _handleConnTest();
  bool _isNintendoCheck();
};

#endif
