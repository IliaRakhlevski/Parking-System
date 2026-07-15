# STM32GpsSimulator

## Description

STM32GpsSimulator emulates a GPS receiver for the Parking-System project.

The simulator periodically changes the current GPS coordinates and provides them to the BeagleBone Green (BBG) over the I²C bus.

---

## Features

- STM32 I²C Slave
- BBG I²C Master support
- Interrupt-driven communication
- Fixed GPS coordinate table
- Periodic coordinate update using TIM10
- Non-blocking data transmission

---

## Communication

### I²C

- STM32 : Slave
- BBG : Master

The BBG requests the current GPS coordinates.

The STM32 prepares the response in `HAL_I2C_AddrCallback()` and transmits it using interrupt mode.

---

## GPS Data

GPS coordinates are stored in a predefined lookup table.

Every timer period the simulator switches to the next coordinate.

This approach provides deterministic and repeatable behaviour during testing.

---

## Timer

TIM10 is used to periodically advance to the next GPS coordinate.

---

## Callbacks

The simulator uses the following HAL callbacks:

- `HAL_TIM_PeriodElapsedCallback()`
- `HAL_I2C_AddrCallback()`
- `HAL_I2C_SlaveTxCpltCallback()`

---

## Build

Generated with STM32CubeMX.

Application code is implemented in the `Core` directory.
