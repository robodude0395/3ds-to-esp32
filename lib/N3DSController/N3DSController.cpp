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

  // Batched input endpoint (preferred - one request per frame)
  _server.on("/i", HTTP_GET, [this]() { _handleInput(); });

  // Legacy individual endpoints (still supported)
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

WebServer& N3DSController::getServer() {
  return _server;
}

void N3DSController::addEndpoint(const char* uri, HTTPMethod method, WebServer::THandlerFunction handler) {
  _server.on(uri, method, handler);
}

// Batched input handler: GET /i?b=XXXX&x=NN&y=NN
// b = hex bitmask: bit0=A, bit1=B, bit2=X, bit3=Y, bit4=L, bit5=R,
//                  bit6=Start, bit7=Select, bit8=Up, bit9=Down, bit10=Left, bit11=Right
void N3DSController::_handleInput() {
  unsigned int btnMask = 0;
  if (_server.hasArg("b")) {
    btnMask = strtoul(_server.arg("b").c_str(), NULL, 16);
  }

  _input.A      = (btnMask & (1 << 0)) != 0;
  _input.B      = (btnMask & (1 << 1)) != 0;
  _input.X      = (btnMask & (1 << 2)) != 0;
  _input.Y      = (btnMask & (1 << 3)) != 0;
  _input.L      = (btnMask & (1 << 4)) != 0;
  _input.R      = (btnMask & (1 << 5)) != 0;
  _input.Start  = (btnMask & (1 << 6)) != 0;
  _input.Select = (btnMask & (1 << 7)) != 0;
  _input.Up     = (btnMask & (1 << 8)) != 0;
  _input.Down   = (btnMask & (1 << 9)) != 0;
  _input.Left   = (btnMask & (1 << 10)) != 0;
  _input.Right  = (btnMask & (1 << 11)) != 0;

  if (_server.hasArg("x")) _input.stickX = constrain(_server.arg("x").toInt(), -100, 100);
  if (_server.hasArg("y")) _input.stickY = constrain(_server.arg("y").toInt(), -100, 100);

  _lastInput = millis();
  if (_callback) _callback(_input);

  _server.send(200, "text/plain", "OK");
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
