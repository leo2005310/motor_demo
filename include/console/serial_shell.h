#pragma once

#include <Arduino.h>

class StepperMotor;

class SerialShell {
 public:
  explicit SerialShell(StepperMotor &motor);
  void begin();
  void update();

 private:
  void printHelp();
  void printMotorStatus();
  void runCommand(String command);

  StepperMotor &motor_;
  String commandLine_;
};
