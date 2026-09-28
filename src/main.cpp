#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cstdlib>

#include "wifi_credentials.h"
#include "stepper_motor.h"
#include "web_page.h"

namespace {

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

#define IN_1 13
#define IN_2 12
#define IN_3 14
#define IN_4 11

WebServer server(80);
StepperMotor motor(IN_1, IN_2, IN_3, IN_4);
String commandLine;

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

void sendStatus() {
  String json = "{\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"motor\":{\"busy\":";
  json += motor.isBusy() ? "true" : "false";
  json += ",\"rpm\":" + String(motor.speedRpm(), 1);
  json += ",\"position\":" + String(static_cast<long long>(motor.positionSteps()));
  json += ",\"remaining\":" + String(motor.remainingSteps()) + "}}";
  server.send(200, "application/json; charset=utf-8", json);
}

bool applyRequestedSpeed() {
  const String argument = server.arg("rpm");
  char *end = nullptr;
  const float rpm = strtof(argument.c_str(), &end);
  if (end == argument.c_str() || *end != '\0' || !motor.setSpeedRpm(rpm)) {
    server.send(400, "application/json",
                "{\"error\":\"rpm must be a number between 1 and 15\"}");
    return false;
  }
  return true;
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", web_page::INDEX_HTML);
  });

  server.on("/api/status", HTTP_GET, sendStatus);

  server.on("/api/motor", HTTP_POST, []() {
    if (!server.hasArg("steps")) {
      server.send(400, "application/json", "{\"error\":\"missing steps\"}");
      return;
    }

    const long steps = server.arg("steps").toInt();
    if (steps == 0 || steps < -1000000L || steps > 1000000L) {
      server.send(400, "application/json", "{\"error\":\"invalid steps\"}");
      return;
    }

    if (server.hasArg("rpm") && !applyRequestedSpeed()) {
      return;
    }

    motor.moveSteps(static_cast<int32_t>(steps));
    sendStatus();
  });

  server.on("/api/motor/speed", HTTP_POST, []() {
    if (!applyRequestedSpeed()) {
      return;
    }
    sendStatus();
  });

  server.on("/api/motor/stop", HTTP_POST, []() {
    motor.stop();
    sendStatus();
  });

  server.onNotFound([]() {
    server.send(404, "text/plain; charset=utf-8", "404 Not Found");
  });

  server.begin();
  Serial.println("Web server started on port 80.");
}

const char *wifiStatusText(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED:
      return "connected";
    case WL_NO_SSID_AVAIL:
      return "SSID not found";
    case WL_CONNECT_FAILED:
      return "connection failed";
    case WL_CONNECTION_LOST:
      return "connection lost";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "idle";
  }
}

void printWifiInfo() {
  Serial.println("\n--- Wi-Fi information ---");
  Serial.printf("Status       : %s\n", wifiStatusText(WiFi.status()));

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("IP information is unavailable until Wi-Fi is connected.");
    return;
  }

  Serial.printf("SSID         : %s\n", WiFi.SSID().c_str());
  Serial.printf("IPv4 address : %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Subnet mask  : %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("Gateway      : %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("DNS 1        : %s\n", WiFi.dnsIP(0).toString().c_str());
  Serial.printf("DNS 2        : %s\n", WiFi.dnsIP(1).toString().c_str());
  Serial.printf("MAC address  : %s\n", WiFi.macAddress().c_str());
  Serial.printf("RSSI         : %d dBm\n", WiFi.RSSI());
  Serial.printf("Channel      : %d\n", WiFi.channel());
  Serial.printf("Web page     : http://%s/\n", WiFi.localIP().toString().c_str());
}

bool connectWifi() {
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 || WIFI_SSID[0] == '\0') {
    Serial.println("Wi-Fi is not configured. Edit include/wifi_credentials.h first.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.printf("Connecting to Wi-Fi \"%s\"", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Connection failed: %s\n", wifiStatusText(WiFi.status()));
    return false;
  }

  Serial.println("Wi-Fi connected.");
  printWifiInfo();
  return true;
}

void printHelp() {
  Serial.println("\nCommands:");
  Serial.println("  wifi       Show Wi-Fi and IP information");
  Serial.println("  motor cw [steps]   Turn clockwise (default: 4096 steps)");
  Serial.println("  motor ccw [steps]  Turn counterclockwise");
  Serial.println("  motor move <steps> Move signed half-steps");
  Serial.println("  motor rev <turns>  Move signed revolutions");
  Serial.println("  motor rpm <1-15>   Set motor speed");
  Serial.println("  motor stop         Stop and release the coils");
  Serial.println("  motor status       Show motor state");
  Serial.println("  reconnect  Disconnect and reconnect to the router");
  Serial.println("  help       Show this help");
}

void printMotorStatus() {
  Serial.printf("Motor: %s, speed %.1f RPM, position %lld, remaining %ld steps\n",
                motor.isBusy() ? "moving" : "stopped", motor.speedRpm(),
                static_cast<long long>(motor.positionSteps()),
                static_cast<long>(motor.remainingSteps()));
}

bool parseMotorNumber(const String &command, const String &prefix,
                      float &value) {
  if (!command.startsWith(prefix)) {
    return false;
  }
  const String argument = command.substring(prefix.length());
  if (argument.isEmpty()) {
    return false;
  }
  char *end = nullptr;
  value = strtof(argument.c_str(), &end);
  return end != argument.c_str() && *end == '\0';
}

void runCommand(String command) {
  command.trim();
  command.toLowerCase();

  if (command == "wifi") {
    printWifiInfo();
  } else if (command == "motor cw") {
    motor.moveSteps(StepperMotor::STEPS_PER_REVOLUTION);
  } else if (command.startsWith("motor cw ")) {
    const long steps = command.substring(9).toInt();
    if (steps > 0) {
      motor.moveSteps(static_cast<int32_t>(steps));
    } else {
      Serial.println("Steps must be greater than zero.");
    }
  } else if (command == "motor ccw") {
    motor.moveSteps(-StepperMotor::STEPS_PER_REVOLUTION);
  } else if (command.startsWith("motor ccw ")) {
    const long steps = command.substring(10).toInt();
    if (steps > 0) {
      motor.moveSteps(-static_cast<int32_t>(steps));
    } else {
      Serial.println("Steps must be greater than zero.");
    }
  } else if (command == "motor stop") {
    motor.stop();
  } else if (command == "motor status") {
    printMotorStatus();
  } else if (command.startsWith("motor move ")) {
    const long steps = command.substring(11).toInt();
    if (steps != 0) {
      motor.moveSteps(static_cast<int32_t>(steps));
    } else {
      Serial.println("Steps must be a non-zero integer.");
    }
  } else if (command.startsWith("motor rev ")) {
    float revolutions = 0.0F;
    if (parseMotorNumber(command, "motor rev ", revolutions) &&
        revolutions != 0.0F) {
      motor.moveRevolutions(revolutions);
    } else {
      Serial.println("Revolutions must be a non-zero number.");
    }
  } else if (command.startsWith("motor rpm ")) {
    float rpm = 0.0F;
    if (!parseMotorNumber(command, "motor rpm ", rpm) ||
        !motor.setSpeedRpm(rpm)) {
      Serial.println("RPM must be between 1 and 15.");
    }
  } else if (command == "reconnect") {
    WiFi.disconnect();
    delay(200);
    connectWifi();
  } else if (command == "help" || command == "?") {
    printHelp();
  } else if (!command.isEmpty()) {
    Serial.printf("Unknown command: %s\n", command.c_str());
    printHelp();
  }

  Serial.print("esp32> ");
}

void processShell() {
  while (Serial.available() > 0) {
    const char input = static_cast<char>(Serial.read());

    if (input == '\r' || input == '\n') {
      if (!commandLine.isEmpty()) {
        Serial.println();
        runCommand(commandLine);
        commandLine.clear();
      }
    } else if ((input == '\b' || input == 127) && !commandLine.isEmpty()) {
      commandLine.remove(commandLine.length() - 1);
      Serial.print("\b \b");
    } else if (isPrintable(input) && commandLine.length() < 64) {
      commandLine += input;
      Serial.print(input);
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  const uint32_t startedAt = millis();
  while (!Serial && millis() - startedAt < 3000) {
    delay(10);
  }

  Serial.println("\nESP32-S3 Wi-Fi Shell");
  Serial.printf("Chip model   : %s\n", ESP.getChipModel());
  Serial.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());

  motor.begin();

  connectWifi();
  setupWebServer();
  printHelp();
  Serial.print("esp32> ");
}

void loop() {
  motor.update();
  server.handleClient();
  processShell();
  delay(1);
}
