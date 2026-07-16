# STM32GpsSimulator

## Description

STM32GpsSimulator emulates a GPS receiver for the Parking-System project.

It periodically updates predefined GPS coordinates and provides them to the BeagleBone Green (BBG) over the I²C bus.

---

## Features

- STM32 HAL
- I²C Slave
- Interrupt-driven I²C communication
- Predefined GPS coordinate table
- Periodic coordinate update using TIM10
- Non-blocking data transmission

---

## Communication

### I²C

- STM32: Slave
- BBG: Master
- 7-bit slave address: `0x42`

The BBG requests the current GPS coordinates over I²C.

The STM32 prepares the response in `HAL_I2C_AddrCallback()` and transmits the data using interrupt mode.

Each transmitted packet contains:

- `float latitude`
- `float longitude`

---

## GPS Data

GPS coordinates are stored in a predefined lookup table.

TIM10 periodically switches to the next coordinate, providing deterministic and repeatable behaviour during testing.

---

## Callbacks

The application uses the following HAL callbacks:

- `HAL_TIM_PeriodElapsedCallback()`
- `HAL_I2C_AddrCallback()`
- `HAL_I2C_ListenCpltCallback()`
- `HAL_I2C_ErrorCallback()`

---

## Project Structure

The project was generated with STM32CubeMX.

Application-specific code is located in the `Core` directory.
