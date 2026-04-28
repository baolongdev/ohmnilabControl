#pragma once

#include <SimpleFOC.h>

extern MagneticSensorI2C sensor;
extern MagneticSensorI2C sensor1;
extern TwoWire I2Cone;
extern TwoWire I2Ctwo;

extern BLDCMotor       motor;
extern BLDCMotor       motor1;
extern BLDCDriver3PWM  driver;
extern BLDCDriver3PWM  driver1;

void motorSetup();
void motorApplyDefaultConfig();
void motorApplyConfig(float pidP, float pidI, float pidD,
                      float lpfTf, float voltLimit, float velLimit);
float motorRuntimeVelocityLimit();
