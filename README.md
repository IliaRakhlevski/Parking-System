# Parking-System

## [Real Time College](https://rt-ed.co.il/) – a multi-disciplinary Real-Time O.S. and Embedded Software Solutions Center, providing consulting, development, integration, training, and support solutions.<br/>

## Linux Embedded Systems Final Project

Course project developed as part of the Linux Embedded Systems course.

The project implements a distributed parking management system built around BeagleBone Green, STM32, and Linux. It combines embedded firmware, multi-process applications, TCP/IP networking, inter-process communication (IPC), and an SQLite database into a complete end-to-end system.

The architecture consists of multiple independent applications communicating through TCP/IP, Shared Memory, unnamed pipes, I²C, and POSIX signals. The system demonstrates modular software design, multi-process programming, concurrent request processing, and integration between Embedded Linux and STM32 hardware.

---

## Project Goals

The main goal of the project is to design and implement a complete distributed system that integrates Embedded Linux applications with STM32 firmware.

The project demonstrates:

- communication between Linux and STM32 hardware;
- multi-process application design on BeagleBone Green;
- inter-process communication using shared memory, shared queues, unnamed pipes, and POSIX signals;
- multi-client TCP communication using an event-driven server;
- concurrent request processing with POSIX threads;
- persistent parking data management using SQLite;
- automatic application startup and supervision through `systemd`;
- reusable and modular software architecture in C and C++.

---

## Key Features

- Distributed architecture built around STM32, BeagleBone Green, and Linux
- STM32 GPS simulator operating as an interrupt-driven I²C slave
- BeagleBone Green client implemented as two cooperating Linux processes
- Unnamed pipe communication between the BBG I²C and TCP processes
- Automatic BBGClient startup through a `systemd` service
- Event-driven multi-client TCP server based on `select()`
- Bidirectional IPC through shared queues stored in System V shared memory
- Dedicated worker threads for network and database request processing
- SQLite-based parking city and parking session management
- Parking cost calculation based on the tariff active when a session starts
- Command-line utility for managing cities and parking prices
- Database notification through a PID file and `SIGUSR1`
- Centralized configuration and logging
- Graceful shutdown and controlled IPC resource cleanup

---

## System Architecture

![Parking-System architecture](Docs/Images/parking-system-architecture.png)
