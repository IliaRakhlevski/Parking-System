# BBGClient

## Description

BBGClient is a Linux daemon running on the BeagleBone Green.

It acts as a gateway between the STM32 GPS simulator and the Parking-System TCP server.

The application consists of two cooperating processes created with `fork()`. The processes communicate through an unnamed POSIX pipe and are automatically started during system boot by `systemd`.

---

## Responsibilities

- Read GPS coordinates from the STM32 over I²C
- Generate parking events
- Forward parking requests to the TCP server
- Receive server responses
- Maintain independent logs for both processes

---

## Architecture

```text
                BeagleBone Green

        +-----------------------------+
        |                             |
        |  TCP Process                |
        |  - TCP Client               |
        |  - Sends requests           |
        |  - Receives responses       |
        |                             |
        |             ▲               |
        |             │ Pipe          |
        |             ▼               |
        |  I²C Process                |
        |  - I²C Master               |
        |  - Reads GPS coordinates    |
        |  - Generates parking events |
        |                             |
        +-----------------------------+

          I²C                     TCP

STM32  <──────────►  BBGClient  <──────────►  TCP Server
```

---

## Process Model

### I²C Process

Responsibilities:

- Opens the Linux I²C device
- Communicates with the STM32 GPS simulator
- Reads GPS coordinates
- Generates parking requests
- Sends requests to the TCP process through an unnamed pipe

### TCP Process

Responsibilities:

- Receives requests from the pipe
- Connects to the TCP server
- Sends parking requests
- Receives server responses
- Logs processing results

---

## Inter-Process Communication

The two processes communicate through an unnamed POSIX pipe created before `fork()`.

The I²C process acts as the producer, while the TCP process acts as the consumer.

---

## Startup

BBGClient runs as a `systemd` service and starts automatically during system boot.

---

## Dependencies

- Config
- Logger
- IpcProtocol
- Linux I²C (`/dev/i2c-*`)
- POSIX API
