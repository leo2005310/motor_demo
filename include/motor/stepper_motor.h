#pragma once

#include <Arduino.h>

class StepperMotor {
 public:
  static constexpr int32_t STEPS_PER_REVOLUTION = 4096;
  static constexpr float MIN_RPM = 1.0F;
  static constexpr float MAX_RPM = 15.0F;

  StepperMotor(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4);

  void begin();
  void update();
  void moveSteps(int32_t steps);
  void moveRevolutions(float revolutions);
  void stop();
  void release();

  bool setSpeedRpm(float rpm);
  float speedRpm() const;
  bool isBusy() const;
  int32_t remainingSteps() const;
  int64_t positionSteps() const;

 private:
  void stepOnce(int8_t direction);
  void writePhase(uint8_t phase);

  uint8_t pins_[4];
  uint8_t phase_ = 0;
  float rpm_ = 10.0F;
  uint32_t stepIntervalUs_ = 0;
  uint32_t nextStepAtUs_ = 0;
  int64_t position_ = 0;
  int64_t target_ = 0;
  bool busy_ = false;
};
