#include <Arduino.h>
#include <ESP32Servo.h>
#include <N3DSController.h>

// --- Pin assignments ---
#define THROTTLE_PIN 4
#define STEERING_PIN 16

// --- Servo pulse widths (microseconds) ---
#define SERVO_MIN     1000
#define SERVO_MAX     2000
#define SERVO_NEUTRAL 1500

// =============================================================
// Tunable parameters (adjustable via 3DS touchscreen GUI)
// =============================================================

// Throttle & steering power (0-100%)
int throttlePercent = 75;
int steeringPercent = 100;

// Reverse flags
bool reverseThrottle = true;
bool reverseSteering = true;

// --- Throttle ramping ---
// Max microseconds the throttle can change per loop iteration.
// At ~50Hz loop rate, rampRate of 20 means 0->full in ~500ms.
// Lower = slower ramp = less current spike.
int throttleRampRate = 5;  // us per loop tick (tunable via /cfg)

// --- Drift mode parameters ---
int driftThrottleLow  = 50;   // low pulse (%)
int driftThrottleHigh = 95;   // high pulse (%)
int driftPulseHighMs = 150;   // time at high throttle (ms)
int driftPulseLowMs  = 100;   // time at low throttle (ms)
// Boost after drift
int boostPercent    = 100;    // throttle % during boost
int boostDurationMs = 500;    // how long boost lasts (ms)

// =============================================================
// Objects
// =============================================================

N3DSController controller;
Servo throttleServo;
Servo steeringServo;

// =============================================================
// Throttle ramping state
// =============================================================

int currentThrottleUs = SERVO_NEUTRAL;  // actual output right now

// Ramp currentThrottleUs toward targetUs by at most rampRate per call
int rampThrottle(int targetUs) {
  int diff = targetUs - currentThrottleUs;

  if (abs(diff) <= throttleRampRate) {
    currentThrottleUs = targetUs;
  } else if (diff > 0) {
    currentThrottleUs += throttleRampRate;
  } else {
    currentThrottleUs -= throttleRampRate;
  }

  return currentThrottleUs;
}

// =============================================================
// Drift state machine
// =============================================================

enum DriftState { DRIFT_OFF, DRIFT_ACTIVE, DRIFT_BOOST };
DriftState driftState = DRIFT_OFF;
unsigned long driftTimer = 0;
bool driftPulseHigh = false;
unsigned long boostStartTime = 0;

// =============================================================
// Helpers
// =============================================================

// Compute throttle microseconds from a signed percentage (-100 to 100)
int throttleToUs(int percent) {
  int offset = (SERVO_NEUTRAL - SERVO_MIN) * abs(percent) / 100;
  if (percent > 0) {
    return reverseThrottle ? SERVO_NEUTRAL - offset : SERVO_NEUTRAL + offset;
  } else if (percent < 0) {
    return reverseThrottle ? SERVO_NEUTRAL + offset : SERVO_NEUTRAL - offset;
  }
  return SERVO_NEUTRAL;
}

// Compute steering microseconds from stick/dpad
int steeringToUs(int stickX, bool left, bool right) {
  int steerRange = (SERVO_NEUTRAL - SERVO_MIN) * steeringPercent / 100;
  int steeringUs = SERVO_NEUTRAL;

  if (stickX != 0) {
    int raw = map(stickX, -100, 100, -steerRange, steerRange);
    steeringUs = SERVO_NEUTRAL + (reverseSteering ? -raw : raw);
  } else if (left) {
    steeringUs = reverseSteering ? SERVO_NEUTRAL + steerRange : SERVO_NEUTRAL - steerRange;
  } else if (right) {
    steeringUs = reverseSteering ? SERVO_NEUTRAL - steerRange : SERVO_NEUTRAL + steerRange;
  }

  return constrain(steeringUs, SERVO_MIN, SERVO_MAX);
}

// =============================================================
// Setup
// =============================================================

void setup() {
  Serial.begin(115200);

  // Attach servos
  throttleServo.attach(THROTTLE_PIN, SERVO_MIN, SERVO_MAX);
  steeringServo.attach(STEERING_PIN, SERVO_MIN, SERVO_MAX);

  // Start at neutral
  throttleServo.writeMicroseconds(SERVO_NEUTRAL);
  steeringServo.writeMicroseconds(SERVO_NEUTRAL);

  controller.begin();

  // --- Config endpoint: set parameters from 3DS GUI ---
  controller.addEndpoint("/cfg", HTTP_GET, []() {
    WebServer& srv = controller.getServer();

    if (srv.hasArg("tp")) throttlePercent = constrain(srv.arg("tp").toInt(), 0, 100);
    if (srv.hasArg("sp")) steeringPercent = constrain(srv.arg("sp").toInt(), 0, 100);
    if (srv.hasArg("rt")) reverseThrottle = srv.arg("rt").toInt() != 0;
    if (srv.hasArg("rs")) reverseSteering = srv.arg("rs").toInt() != 0;
    if (srv.hasArg("dl")) driftThrottleLow = constrain(srv.arg("dl").toInt(), 0, 100);
    if (srv.hasArg("dh")) driftThrottleHigh = constrain(srv.arg("dh").toInt(), 0, 100);
    if (srv.hasArg("dph")) driftPulseHighMs = constrain(srv.arg("dph").toInt(), 10, 2000);
    if (srv.hasArg("dpl")) driftPulseLowMs = constrain(srv.arg("dpl").toInt(), 10, 2000);
    if (srv.hasArg("bp")) boostPercent = constrain(srv.arg("bp").toInt(), 0, 100);
    if (srv.hasArg("bd")) boostDurationMs = constrain(srv.arg("bd").toInt(), 0, 5000);
    if (srv.hasArg("rr")) throttleRampRate = constrain(srv.arg("rr").toInt(), 1, 500);

    char buf[300];
    snprintf(buf, sizeof(buf),
      "tp=%d&sp=%d&rt=%d&rs=%d&dl=%d&dh=%d&dph=%d&dpl=%d&bp=%d&bd=%d&rr=%d",
      throttlePercent, steeringPercent,
      reverseThrottle ? 1 : 0, reverseSteering ? 1 : 0,
      driftThrottleLow, driftThrottleHigh,
      driftPulseHighMs, driftPulseLowMs,
      boostPercent, boostDurationMs,
      throttleRampRate);
    srv.send(200, "text/plain", buf);
  });

  // --- Read-only config endpoint ---
  controller.addEndpoint("/cfg_get", HTTP_GET, []() {
    WebServer& srv = controller.getServer();
    char buf[300];
    snprintf(buf, sizeof(buf),
      "tp=%d&sp=%d&rt=%d&rs=%d&dl=%d&dh=%d&dph=%d&dpl=%d&bp=%d&bd=%d&rr=%d",
      throttlePercent, steeringPercent,
      reverseThrottle ? 1 : 0, reverseSteering ? 1 : 0,
      driftThrottleLow, driftThrottleHigh,
      driftPulseHighMs, driftPulseLowMs,
      boostPercent, boostDurationMs,
      throttleRampRate);
    srv.send(200, "text/plain", buf);
  });

  Serial.println("[RC Kart] Ready - Mario Kart Mode");
  Serial.printf("[RC Kart] Throttle ramp rate: %d us/tick\n", throttleRampRate);
}

// =============================================================
// Loop
// =============================================================

void loop() {
  controller.update();

  const N3DSInput& input = controller.getInput();
  unsigned long now = millis();

  // --- Drift state machine ---
  bool driftButton = input.L;  // Hold L to drift

  switch (driftState) {
    case DRIFT_OFF:
      if (driftButton && input.A) {
        driftState = DRIFT_ACTIVE;
        driftPulseHigh = true;
        driftTimer = now;
        Serial.println("[Drift] START");
      }
      break;

    case DRIFT_ACTIVE:
      if (!driftButton) {
        driftState = DRIFT_BOOST;
        boostStartTime = now;
        Serial.println("[Drift] BOOST!");
      }
      break;

    case DRIFT_BOOST:
      if (now - boostStartTime >= (unsigned long)boostDurationMs) {
        driftState = DRIFT_OFF;
        Serial.println("[Drift] boost ended");
      }
      break;
  }

  // --- Compute target throttle ---
  int targetThrottleUs = SERVO_NEUTRAL;

  if (driftState == DRIFT_BOOST) {
    targetThrottleUs = throttleToUs(boostPercent);
  } else if (driftState == DRIFT_ACTIVE) {
    unsigned long elapsed = now - driftTimer;
    if (driftPulseHigh) {
      targetThrottleUs = throttleToUs(driftThrottleHigh);
      if (elapsed >= (unsigned long)driftPulseHighMs) {
        driftPulseHigh = false;
        driftTimer = now;
      }
    } else {
      targetThrottleUs = throttleToUs(driftThrottleLow);
      if (elapsed >= (unsigned long)driftPulseLowMs) {
        driftPulseHigh = true;
        driftTimer = now;
      }
    }
  } else {
    // Normal driving
    if (input.A) {
      targetThrottleUs = throttleToUs(throttlePercent);
    } else if (input.B) {
      targetThrottleUs = throttleToUs(-throttlePercent);
    }
  }

  // Apply ramping to avoid power supply brown-out
  int outputThrottleUs = rampThrottle(targetThrottleUs);
  throttleServo.writeMicroseconds(outputThrottleUs);

  // --- Steering (no ramping needed, instant is fine) ---
  int steeringUs = steeringToUs(input.stickX, input.Left, input.Right);
  steeringServo.writeMicroseconds(steeringUs);
}
