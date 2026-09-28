#include "network/wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "wifi_credentials.h"

namespace network {
namespace {

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

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

}  // namespace

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

void reconnectWifi() {
  WiFi.disconnect();
  delay(200);
  connectWifi();
}

}  // namespace network
