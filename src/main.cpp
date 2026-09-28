#include <Arduino.h>

#include "config/board_config.h"
#include "console/serial_shell.h"
#include "motor/stepper_motor.h"
#include "network/wifi_manager.h"
#include "web/web_server.h"

namespace {

StepperMotor motor(board_config::MOTOR_IN1, board_config::MOTOR_IN2,
                   board_config::MOTOR_IN3, board_config::MOTOR_IN4);
MotorWebServer webServer(motor);
SerialShell serialShell(motor);

}  // namespace

void setup() {
  Serial.begin(board_config::SERIAL_BAUD);

  const uint32_t startedAt = millis();
  while (!Serial && millis() - startedAt < 3000) {
    delay(10);
  }

  Serial.println("\nESP32-S3 Wi-Fi Shell");
  Serial.printf("Chip model   : %s\n", ESP.getChipModel());
  Serial.printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());

  motor.begin();

  network::connectWifi();
  webServer.begin();
  serialShell.begin();
}

void loop() {
  motor.update();
  webServer.update();
  serialShell.update();
  delay(1);
}
