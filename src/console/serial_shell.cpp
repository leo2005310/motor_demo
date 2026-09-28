#include "console/serial_shell.h"

#include <cstdlib>

#include "motor/stepper_motor.h"
#include "network/wifi_manager.h"

namespace {

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

}  // namespace

SerialShell::SerialShell(StepperMotor &motor) : motor_(motor) {}

void SerialShell::begin() {
  printHelp();
  Serial.print("esp32> ");
}

void SerialShell::printHelp() {
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

void SerialShell::printMotorStatus() {
  Serial.printf("Motor: %s, speed %.1f RPM, position %lld, remaining %ld steps\n",
                motor_.isBusy() ? "moving" : "stopped", motor_.speedRpm(),
                static_cast<long long>(motor_.positionSteps()),
                static_cast<long>(motor_.remainingSteps()));
}

void SerialShell::runCommand(String command) {
  command.trim();
  command.toLowerCase();

  if (command == "wifi") {
    network::printWifiInfo();
  } else if (command == "motor cw") {
    motor_.moveSteps(StepperMotor::STEPS_PER_REVOLUTION);
  } else if (command.startsWith("motor cw ")) {
    const long steps = command.substring(9).toInt();
    if (steps > 0) {
      motor_.moveSteps(static_cast<int32_t>(steps));
    } else {
      Serial.println("Steps must be greater than zero.");
    }
  } else if (command == "motor ccw") {
    motor_.moveSteps(-StepperMotor::STEPS_PER_REVOLUTION);
  } else if (command.startsWith("motor ccw ")) {
    const long steps = command.substring(10).toInt();
    if (steps > 0) {
      motor_.moveSteps(-static_cast<int32_t>(steps));
    } else {
      Serial.println("Steps must be greater than zero.");
    }
  } else if (command == "motor stop") {
    motor_.stop();
  } else if (command == "motor status") {
    printMotorStatus();
  } else if (command.startsWith("motor move ")) {
    const long steps = command.substring(11).toInt();
    if (steps != 0) {
      motor_.moveSteps(static_cast<int32_t>(steps));
    } else {
      Serial.println("Steps must be a non-zero integer.");
    }
  } else if (command.startsWith("motor rev ")) {
    float revolutions = 0.0F;
    if (parseMotorNumber(command, "motor rev ", revolutions) &&
        revolutions != 0.0F) {
      motor_.moveRevolutions(revolutions);
    } else {
      Serial.println("Revolutions must be a non-zero number.");
    }
  } else if (command.startsWith("motor rpm ")) {
    float rpm = 0.0F;
    if (!parseMotorNumber(command, "motor rpm ", rpm) ||
        !motor_.setSpeedRpm(rpm)) {
      Serial.println("RPM must be between 1 and 15.");
    }
  } else if (command == "reconnect") {
    network::reconnectWifi();
  } else if (command == "help" || command == "?") {
    printHelp();
  } else if (!command.isEmpty()) {
    Serial.printf("Unknown command: %s\n", command.c_str());
    printHelp();
  }

  Serial.print("esp32> ");
}

void SerialShell::update() {
  while (Serial.available() > 0) {
    const char input = static_cast<char>(Serial.read());

    if (input == '\r' || input == '\n') {
      if (!commandLine_.isEmpty()) {
        Serial.println();
        runCommand(commandLine_);
        commandLine_.clear();
      }
    } else if ((input == '\b' || input == 127) && !commandLine_.isEmpty()) {
      commandLine_.remove(commandLine_.length() - 1);
      Serial.print("\b \b");
    } else if (isPrintable(input) && commandLine_.length() < 64) {
      commandLine_ += input;
      Serial.print(input);
    }
  }
}

