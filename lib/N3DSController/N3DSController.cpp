#include "N3DSController.h"

// Nintendo connectivity check response
static const char* CONN_RESPONSE =
  "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
  "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\r\n"
  "<html>\r\n<head>\r\n<title>HTML Page</title>\r\n</head>\r\n"
  "<body bgcolor=\"#FFFFFF\">\r\nThis is test.\r\n</body>\r\n</html>\r\n";

void N3DSController::setAP(const char* ssid, const char* password) {
  _ssid = ssid;
  _pass = password;
}

void N3DSController::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(_ssid, _pass);
  _dns.start(53, "*", WiFi.softAPIP());

  _server.on("/b", HTTP_GET, [this]() { _handleButton(); });
  _server.on("/s", HTTP_GET, [this]() { _handleStick(); });

  // Handle connectivity checks and anything else
  _server.on("/", HTTP_ANY, [this]() { _handleConnTest(); });
  _server.onNotFound([this]() { _handleConnTest(); });

  _server.begin();

  Serial.printf("[N3DS] AP \"%s\" @ %s\n", _ssid, WiFi.softAPIP().toString().c_str());
}

void N3DSController::update() {
  _dns.processNextRequest();
  _server.handleClient();

  // Auto-clear on timeout
  if (_timeout > 0 && _lastInput > 0 && millis() - _lastInput > _timeout) {
    _input.clear();
    _lastInput = 0;
  }
}

const N3DSInput& N3DSController::getInput() const {
  return _input;
}

bool N3DSController::isConnected(unsigned long timeoutMs) const {
  if (_lastInput == 0) return false;
  return (millis() - _lastInput) < timeoutMs;
}

void N3DSController::onInput(N3DSInputCallback callback) {
  _callback = callback;
}

void N3DSController::setInputTimeout(unsigned long ms) {
  _timeout = ms;
}

void N3DSController::_handleButton() {
  String id = _server.arg("i");
  bool pressed = _server.arg("s").toInt() == 1;

  if (id == "A") _input.A = pressed;
  else if (id == "B") _input.B = pressed;
  else if (id == "X") _input.X = pressed;
  else if (id == "Y") _input.Y = pressed;
  else if (id == "L") _input.L = pressed;
  else if (id == "R") _input.R = pressed;
  else if (id == "ST") _input.Start = pressed;
  else if (id == "SE") _input.Select = pressed;
  else if (id == "U") _input.Up = pressed;
  else if (id == "D") _input.Down = pressed;
  else if (id == "LT") _input.Left = pressed;
  else if (id == "RT") _input.Right = pressed;

  _lastInput = millis();
  if (_callback) _callback(_input);

  _server.send(200, "text/plain", "OK");
}

void N3DSController::_handleStick() {
  _input.stickX = constrain(_server.arg("x").toInt(), -100, 100);
  _input.stickY = constrain(_server.arg("y").toInt(), -100, 100);

  _lastInput = millis();
  if (_callback) _callback(_input);

  _server.send(200, "text/plain", "OK");
}

void N3DSController::_handleConnTest() {
  if (_isNintendoCheck()) {
    _server.sendHeader("X-Organization", "Nintendo");
    _server.send(200, "text/html", CONN_RESPONSE);
  } else {
    _server.send(200, "text/plain", "N3DS Controller Server");
  }
}

bool N3DSController::_isNintendoCheck() {
  String host = _server.hostHeader();
  return host.indexOf("nintendo") >= 0 || host.indexOf("conntest") >= 0;
}
