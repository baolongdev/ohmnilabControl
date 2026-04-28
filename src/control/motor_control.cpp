#include "motor_control.h"
#include "../config.h"

// Sensor objects - moi cai dung mot I2C bus rieng
MagneticSensorI2C sensor  = MagneticSensorI2C(AS5600_I2C);
MagneticSensorI2C sensor1 = MagneticSensorI2C(AS5600_I2C);
TwoWire I2Cone = TwoWire(0);
TwoWire I2Ctwo = TwoWire(1);

// Motor & driver objects
BLDCMotor      motor   = BLDCMotor(pole_pairs);
BLDCMotor      motor1  = BLDCMotor(pole_pairs);
BLDCDriver3PWM driver  = BLDCDriver3PWM(32, 33, 25, 22);
BLDCDriver3PWM driver1 = BLDCDriver3PWM(26, 27, 14, 12);

void motorApplyConfig(float pidP, float pidI, float pidD,
                      float lpfTf, float voltLimit, float velLimit) {
  motor.PID_velocity.P  = pidP;
  motor.PID_velocity.I  = pidI;
  motor.PID_velocity.D  = pidD;
  motor1.PID_velocity.P = pidP;
  motor1.PID_velocity.I = pidI;
  motor1.PID_velocity.D = pidD;

  motor.LPF_velocity.Tf  = lpfTf;
  motor1.LPF_velocity.Tf = lpfTf;

  motor.voltage_limit   = voltLimit;
  motor1.voltage_limit  = voltLimit;
  motor.velocity_limit  = velLimit;
  motor1.velocity_limit = velLimit;

  motor.PID_velocity.limit  = voltLimit;
  motor1.PID_velocity.limit = voltLimit;
}

void motorApplyDefaultConfig() {
  motorApplyConfig(PID_P, PID_I, PID_D, LPF_Tf, voltage_limit, velocity_limit);
}

float motorRuntimeVelocityLimit() {
  return motor.velocity_limit > 0.0f ? motor.velocity_limit : velocity_limit;
}

void motorSetup() {
  // I2C & sensor init
  I2Cone.begin(I2C0_SDA, I2C0_SCL, 400000);
  I2Ctwo.begin(I2C1_SDA, I2C1_SCL, 400000);
  sensor.init(&I2Cone);
  sensor1.init(&I2Ctwo);

  motor.linkSensor(&sensor);
  motor1.linkSensor(&sensor1);

  // Driver init
  driver.voltage_power_supply  = voltage_power_supply;
  driver.init();
  driver1.voltage_power_supply = voltage_power_supply;
  driver1.init();

  motor.linkDriver(&driver);
  motor1.linkDriver(&driver1);

  // FOC mode & control type
  motor.foc_modulation  = FOCModulationType::SpaceVectorPWM;
  motor1.foc_modulation = FOCModulationType::SpaceVectorPWM;

  motor.controller  = MotionControlType::velocity;
  motor1.controller = MotionControlType::velocity;

  motorApplyDefaultConfig();

  // Init & align
  motor.init();
  motor1.init();

  motor.initFOC();
  motor1.initFOC();
}
