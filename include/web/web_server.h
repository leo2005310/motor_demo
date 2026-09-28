#pragma once

#include <WebServer.h>

class StepperMotor;

// HTTP adapter: translates requests into motor operations. Owns no GPIO state.
class MotorWebServer {
 public:
  explicit MotorWebServer(StepperMotor &motor);
  void begin();
  void update();

 private:
  void sendStatus();
  bool applyRequestedSpeed();

  StepperMotor &motor_;
  WebServer server_{80};
};
