#include <Arduino.h>
#include <N3DSController.h>

N3DSController controller;

// Called every time the 3DS sends an input
void onInput(const N3DSInput& input) {
  // Example: print what's happening
  if (input.A) Serial.println("A pressed");
  if (input.B) Serial.println("B pressed");
  if (input.stickX != 0 || input.stickY != 0) {
    Serial.printf("Stick: %d, %d\n", input.stickX, input.stickY);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  // Optional: change AP name/password (default: "3DS_Controller" / "12345678")
  // controller.setAP("MyRobot", "password123");

  // Optional: set a callback
  controller.onInput(onInput);

  // Optional: auto-clear inputs after 2 seconds of silence (default)
  // controller.setInputTimeout(2000);

  controller.begin();
}

void loop() {
  controller.update();

  // Read inputs directly
  const N3DSInput& input = controller.getInput();

  // Example: LED on when any button pressed
  digitalWrite(2, input.anyActive() ? HIGH : LOW);

  // Example: use stick for motor control
  // int leftMotor  = input.stickY + input.stickX;
  // int rightMotor = input.stickY - input.stickX;
  // setMotors(leftMotor, rightMotor);
}
