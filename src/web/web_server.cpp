#include "web/web_server.h"

#include <WiFi.h>
#include <cstdlib>

#include "motor/stepper_motor.h"

namespace {

// Linker symbols generated from board_build.embed_txtfiles in platformio.ini.
extern const char INDEX_HTML[] asm("_binary_web_index_html_start");
extern const char STYLE_CSS[] asm("_binary_web_style_css_start");
extern const char APP_JS[] asm("_binary_web_app_js_start");

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 4);
  for (size_t i = 0; i < value.length(); ++i) {
    const char ch = value[i];
    if (ch == '"' || ch == '\\') {
      escaped += '\\';
    }
    escaped += ch;
  }
  return escaped;
}

}  // namespace

MotorWebServer::MotorWebServer(StepperMotor &motor) : motor_(motor) {}

void MotorWebServer::sendStatus() {
  String json = "{\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"motor\":{\"busy\":";
  json += motor_.isBusy() ? "true" : "false";
  json += ",\"rpm\":" + String(motor_.speedRpm(), 1);
  json += ",\"position\":" + String(static_cast<long long>(motor_.positionSteps()));
  json += ",\"remaining\":" + String(motor_.remainingSteps()) + "}}";
  server_.send(200, "application/json; charset=utf-8", json);
}

bool MotorWebServer::applyRequestedSpeed() {
  const String argument = server_.arg("rpm");
  char *end = nullptr;
  const float rpm = strtof(argument.c_str(), &end);
  if (end == argument.c_str() || *end != '\0' || !motor_.setSpeedRpm(rpm)) {
    server_.send(400, "application/json",
                "{\"error\":\"rpm must be a number between 1 and 15\"}");
    return false;
  }
  return true;
}

void MotorWebServer::begin() {
  server_.on("/", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server_.on("/style.css", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/css; charset=utf-8", STYLE_CSS);
  });

  server_.on("/app.js", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "application/javascript; charset=utf-8", APP_JS);
  });

  server_.on("/api/status", HTTP_GET, [this]() { sendStatus(); });

  server_.on("/api/motor", HTTP_POST, [this]() {
    if (!server_.hasArg("steps")) {
      server_.send(400, "application/json", "{\"error\":\"missing steps\"}");
      return;
    }

    const long steps = server_.arg("steps").toInt();
    if (steps == 0 || steps < -1000000L || steps > 1000000L) {
      server_.send(400, "application/json", "{\"error\":\"invalid steps\"}");
      return;
    }

    if (server_.hasArg("rpm") && !applyRequestedSpeed()) {
      return;
    }

    motor_.moveSteps(static_cast<int32_t>(steps));
    sendStatus();
  });

  server_.on("/api/motor/speed", HTTP_POST, [this]() {
    if (!applyRequestedSpeed()) {
      return;
    }
    sendStatus();
  });

  server_.on("/api/motor/stop", HTTP_POST, [this]() {
    motor_.stop();
    sendStatus();
  });

  server_.onNotFound([this]() {
    server_.send(404, "text/plain; charset=utf-8", "404 Not Found");
  });

  server_.begin();
  Serial.println("Web server started on port 80.");
}

void MotorWebServer::update() { server_.handleClient(); }
