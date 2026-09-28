#include "stepper_motor.h"

#include <cmath>

namespace {

// 28BYJ-48 half-step sequence: IN1, IN2, IN3, IN4.
constexpr uint8_t HALF_STEP_SEQUENCE[8][4] = {
    {HIGH, LOW, LOW, LOW},  {HIGH, HIGH, LOW, LOW},
    {LOW, HIGH, LOW, LOW},  {LOW, HIGH, HIGH, LOW},
    {LOW, LOW, HIGH, LOW},  {LOW, LOW, HIGH, HIGH},
    {LOW, LOW, LOW, HIGH},  {HIGH, LOW, LOW, HIGH},
};

}  // namespace

StepperMotor::StepperMotor(uint8_t in1, uint8_t in2, uint8_t in3,
                           uint8_t in4)
    : pins_{in1, in2, in3, in4} {}

void StepperMotor::begin() {
  for (const uint8_t pin : pins_) {
    pinMode(pin, OUTPUT);
  }
  setSpeedRpm(rpm_);
  release();
}

void StepperMotor::update() {
  if (!busy_) {
    return;
  }

  // Catch up after Wi-Fi handling delays, but cap each batch so HTTP remains
  // responsive.
  uint8_t stepsProcessed = 0;
  uint32_t now = micros();
  while (busy_ && static_cast<int32_t>(now - nextStepAtUs_) >= 0 &&
         stepsProcessed < 8) {
    const int8_t direction = target_ > position_ ? 1 : -1;
    stepOnce(direction);
    position_ += direction;
    ++stepsProcessed;

    if (position_ == target_) {
      busy_ = false;
      release();
      break;
    }

    nextStepAtUs_ += stepIntervalUs_;
    now = micros();
  }
}

void StepperMotor::moveSteps(int32_t steps) {
  if (steps == 0) {
    stop();
    return;
  }

  target_ = position_ + steps;
  busy_ = true;
  nextStepAtUs_ = micros();
}

void StepperMotor::moveRevolutions(float revolutions) {
  moveSteps(static_cast<int32_t>(
      lroundf(revolutions * static_cast<float>(STEPS_PER_REVOLUTION))));
}

void StepperMotor::stop() {
  target_ = position_;
  busy_ = false;
  release();
}

void StepperMotor::release() {
  for (const uint8_t pin : pins_) {
    digitalWrite(pin, LOW);
  }
}

bool StepperMotor::setSpeedRpm(float rpm) {
  if (!std::isfinite(rpm) || rpm < MIN_RPM || rpm > MAX_RPM) {
    return false;
  }

  rpm_ = rpm;
  stepIntervalUs_ = static_cast<uint32_t>(
      60000000.0F / (rpm_ * static_cast<float>(STEPS_PER_REVOLUTION)));
  return true;
}

float StepperMotor::speedRpm() const { return rpm_; }

bool StepperMotor::isBusy() const { return busy_; }

int32_t StepperMotor::remainingSteps() const {
  const int64_t remaining = target_ - position_;
  if (remaining > INT32_MAX) {
    return INT32_MAX;
  }
  if (remaining < INT32_MIN) {
    return INT32_MIN;
  }
  return static_cast<int32_t>(remaining);
}

int64_t StepperMotor::positionSteps() const { return position_; }

void StepperMotor::stepOnce(int8_t direction) {
  phase_ = static_cast<uint8_t>((phase_ + (direction > 0 ? 1 : 7)) & 0x07);
  writePhase(phase_);
}

void StepperMotor::writePhase(uint8_t phase) {
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(pins_[i], HALF_STEP_SEQUENCE[phase][i]);
  }
}
