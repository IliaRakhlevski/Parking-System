# BeagleBone Green systemd Service Setup

## Overview

This document describes how to install and manage the Parking System BBG Client as a `systemd` service.

The service starts the BBG client automatically during system boot, eliminating the need to launch the application manually.

The BBG client executable creates two processes:

- TCP process
- I2C process

Both processes are managed as part of the same `systemd` service.

---

## Runtime Directory Structure

The application files are stored in `/root` on the BeagleBone Green:

```text
/root
├── bbg_client_arm
├── Config
│   └── bbg_client.conf
├── Logs
│   ├── bbg_i2c.log
│   └── bbg_tcp.log
└── Systemd
    └── bbg-client.service
```

The BBG client uses relative paths.

The configuration file is accessed as:

```c
#define BBG_CLIENT_CONFIG_FILE_PATH "Config/bbg_client.conf"
```

The log file paths are configured as:

```ini
tcp_log_file = Logs/bbg_tcp.log
i2c_log_file = Logs/bbg_i2c.log
```

For this reason, the service working directory must be `/root`.

---

## systemd Unit File

Repository copy:

```text
BbgClient/Systemd/bbg-client.service
```

Runtime installation location:

```text
/etc/systemd/system/bbg-client.service
```

Unit file contents:

```ini
#
# Parking System BBG Client systemd service.
#
# This unit starts the BeagleBone Green client automatically during
# system boot and manages it as a system service.
#
# Repository location:
#     BbgClient/Systemd/bbg-client.service
#
# Installation location on the BeagleBone Green:
#     /etc/systemd/system/bbg-client.service
#

[Unit]
Description=Parking System BeagleBone Green Client
Wants=network-online.target
After=network-online.target

[Service]
Type=simple

# The BBG client uses relative paths for its configuration and log files.
WorkingDirectory=/root

# Main BBG client executable.
ExecStart=/root/bbg_client_arm

# Restart the service if the main process exits unexpectedly.
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

---

## Unit File Explanation

### `[Unit]`

```ini
Description=Parking System BeagleBone Green Client
```

Provides a human-readable service description.

It is displayed by commands such as:

```bash
systemctl status bbg-client.service
```

---

```ini
Wants=network-online.target
```

Requests the network-online target as a soft dependency.

The BBG client requires network access because its TCP process connects to the Parking System TCP Server.

---

```ini
After=network-online.target
```

Defines startup ordering.

The BBG client is started after systemd considers network configuration complete.

This does not guarantee that the external TCP Server is already running. If the server is unavailable, the BBG Client logs a connection error and continues processing later events.

---

### `[Service]`

```ini
Type=simple
```

The process started by `ExecStart` remains in the foreground and is treated as the main service process.

The BBG Client performs an internal `fork()` to create its two worker processes:

```text
bbg_client_arm
├── TCP process
└── I2C process
```

The existing `fork()` is part of the application architecture and is not used for daemonization.

Both processes remain inside the same systemd control group.

---

```ini
WorkingDirectory=/root
```

Sets `/root` as the current working directory before the application starts.

This allows the program to resolve relative paths correctly:

```text
Config/bbg_client.conf
```

becomes:

```text
/root/Config/bbg_client.conf
```

and:

```text
Logs/bbg_tcp.log
Logs/bbg_i2c.log
```

become:

```text
/root/Logs/bbg_tcp.log
/root/Logs/bbg_i2c.log
```

---

```ini
ExecStart=/root/bbg_client_arm
```

Specifies the absolute path of the executable started by systemd.

Absolute paths should be used in systemd unit files to avoid dependency on shell environment variables such as `PATH`.

---

```ini
Restart=on-failure
```

Restarts the service if the main process terminates unexpectedly.

Examples include:

- non-zero exit code;
- segmentation fault;
- unexpected process termination.

A normal administrative stop does not cause an automatic restart:

```bash
systemctl stop bbg-client.service
```

---

```ini
RestartSec=3
```

Waits three seconds before restarting the service after a failure.

This prevents rapid restart loops if the application repeatedly fails.

---

### `[Install]`

```ini
WantedBy=multi-user.target
```

Connects the service to the normal multi-user system startup target.

When the service is enabled, systemd creates a symbolic link under:

```text
/etc/systemd/system/multi-user.target.wants/
```

This causes the BBG Client to start automatically during boot.

---

# Installation Procedure

## 1. Create the Project Copy

Create a directory for the service file:

```bash
mkdir -p /root/Systemd
```

Create the unit file:

```bash
nano /root/Systemd/bbg-client.service
```

Paste the unit contents shown above.

Check the file:

```bash
cat /root/Systemd/bbg-client.service
```

---

## 2. Install the Unit File

Copy the project copy into the systemd unit directory:

```bash
cp /root/Systemd/bbg-client.service \
   /etc/systemd/system/bbg-client.service
```

The file is copied rather than moved so that:

- `/root/Systemd/bbg-client.service` remains the source version;
- `/etc/systemd/system/bbg-client.service` is the installed runtime version;
- the source version can be committed to GitHub.

Verify the installed file:

```bash
ls -l /etc/systemd/system/bbg-client.service
```

Example output:

```text
-rw-r--r-- 1 root root 788 Jun 30 21:38 /etc/systemd/system/bbg-client.service
```

---

## 3. Reload systemd Configuration

After installing or modifying a unit file, reload the systemd configuration:

```bash
systemctl daemon-reload
```

This command tells the running systemd manager to reread all unit files.

It does not start or stop the BBG Client.

---

## 4. Check the Service Before Starting It

```bash
systemctl status bbg-client.service
```

Expected state before the first start:

```text
Loaded: loaded (/etc/systemd/system/bbg-client.service; disabled)
Active: inactive (dead)
```

Meaning:

- the unit file was loaded successfully;
- the service is not currently running;
- automatic boot startup is not enabled yet.

---

## 5. Start the Service Manually

```bash
systemctl start bbg-client.service
```

Check its status:

```bash
systemctl status bbg-client.service
```

Expected result:

```text
Active: active (running)
```

The status should also show two tasks or two processes:

```text
CGroup: /system.slice/bbg-client.service
        ├──989 /root/bbg_client_arm
        └──990 /root/bbg_client_arm
```

The exact process IDs will be different.

This confirms that both the TCP and I2C processes belong to the same systemd service.

---

## 6. Verify the Application Logs

Check the I2C process log:

```bash
tail -f /root/Logs/bbg_i2c.log
```

Example output:

```text
[INFO ] GPS coordinates received: latitude 10.100000, longitude 10.150000.
[INFO ] Generated START_PARKING request 13 for vehicle 99-666-77.
```

Check the TCP process log:

```bash
tail -f /root/Logs/bbg_tcp.log
```

If the TCP Server is not running, messages such as the following are expected:

```text
[ERROR] Failed to connect to TCP server.
```

This does not indicate a systemd failure. It only means that the external TCP Server is unavailable.

Press `Ctrl+C` to stop following a log file.

---

## 7. Stop the Service

```bash
systemctl stop bbg-client.service
```

Check the status:

```bash
systemctl status bbg-client.service
```

Expected result:

```text
Active: inactive (dead)
```

Systemd stops both BBG Client processes because they belong to the same service control group.

Example systemd messages:

```text
Stopping bbg-client.service - Parking System BeagleBone Green Client...
bbg-client.service: Deactivated successfully.
Stopped bbg-client.service - Parking System BeagleBone Green Client.
```

---

## 8. Enable Automatic Startup

Enable the service at boot:

```bash
systemctl enable bbg-client.service
```

Example output:

```text
Created symlink '/etc/systemd/system/multi-user.target.wants/bbg-client.service'
→ '/etc/systemd/system/bbg-client.service'.
```

Check whether automatic startup is enabled:

```bash
systemctl is-enabled bbg-client.service
```

Expected output:

```text
enabled
```

The `enable` command configures boot startup but does not necessarily start an inactive service immediately.

To enable and start a service in a single command, use:

```bash
systemctl enable --now bbg-client.service
```

This combined command was not required during the initial setup because the service was started and tested separately first.

---

## 9. Verify Automatic Startup After Reboot

Reboot the BeagleBone Green:

```bash
reboot
```

After reconnecting to the board, verify the service:

```bash
systemctl status bbg-client.service
```

Expected result:

```text
Active: active (running)
```

Check that both worker processes exist:

```bash
ps -ef | grep '[b]bg_client_arm'
```

Check the latest I2C log entries:

```bash
tail -n 20 /root/Logs/bbg_i2c.log
```

Check the latest TCP log entries:

```bash
tail -n 20 /root/Logs/bbg_tcp.log
```

This confirms that the BBG Client was started automatically during system boot.

---

# Service Management Commands

## Start

```bash
systemctl start bbg-client.service
```

Starts the service immediately.

---

## Stop

```bash
systemctl stop bbg-client.service
```

Stops both BBG Client processes.

---

## Restart

```bash
systemctl restart bbg-client.service
```

Stops and starts the service again.

Use this after replacing the executable or changing runtime configuration.

---

## Display Status

```bash
systemctl status bbg-client.service
```

Shows:

- loaded unit path;
- enabled or disabled state;
- active or inactive state;
- main process ID;
- child processes;
- recent service messages.

---

## Enable Boot Startup

```bash
systemctl enable bbg-client.service
```

Configures automatic startup during system boot.

---

## Disable Boot Startup

```bash
systemctl disable bbg-client.service
```

Removes the boot-start symbolic link.

The service unit file itself is not deleted.

---

## Check Boot Startup State

```bash
systemctl is-enabled bbg-client.service
```

Possible results include:

```text
enabled
disabled
```

---

## Reload Unit Files

```bash
systemctl daemon-reload
```

Required after changing:

```text
/etc/systemd/system/bbg-client.service
```

It is not normally required after modifying only:

- `bbg_client_arm`;
- `Config/bbg_client.conf`;
- log files.

---

# Updating the Application

After building a new ARM executable, stop the service:

```bash
systemctl stop bbg-client.service
```

Replace the executable:

```bash
cp bbg_client_arm /root/bbg_client_arm
```

Ensure it is executable:

```bash
chmod +x /root/bbg_client_arm
```

Start the service again:

```bash
systemctl start bbg-client.service
```

Check the status:

```bash
systemctl status bbg-client.service
```

If only the executable or configuration file was changed, `systemctl daemon-reload` is not required.

It is required only when the unit file itself changes.

---

# Updating the Unit File

Edit the repository copy:

```bash
nano /root/Systemd/bbg-client.service
```

Install the updated copy:

```bash
cp /root/Systemd/bbg-client.service \
   /etc/systemd/system/bbg-client.service
```

Reload systemd:

```bash
systemctl daemon-reload
```

Restart the service:

```bash
systemctl restart bbg-client.service
```

Verify:

```bash
systemctl status bbg-client.service
```

---

# Troubleshooting

## Service Is Inactive

Check:

```bash
systemctl status bbg-client.service
```

Start it:

```bash
systemctl start bbg-client.service
```

---

## Configuration File Is Not Found

Verify the configured working directory:

```ini
WorkingDirectory=/root
```

Verify the file exists:

```bash
ls -l /root/Config/bbg_client.conf
```

The program expects:

```text
Config/bbg_client.conf
```

relative to `/root`.

---

## Log Files Are Not Created

Verify the log directory:

```bash
ls -ld /root/Logs
```

Create it if necessary:

```bash
mkdir -p /root/Logs
```

Verify the configuration contains:

```ini
tcp_log_file = Logs/bbg_tcp.log
i2c_log_file = Logs/bbg_i2c.log
```

---

## TCP Connection Errors

Example:

```text
Failed to connect to TCP server.
```

Verify that the TCP Server is running and reachable.

Check network connectivity:

```bash
ping 192.168.137.100
```

Check the configured server address:

```bash
cat /root/Config/bbg_client.conf
```

A TCP connection error does not necessarily mean the systemd service failed.

---

## I2C Read Errors

Verify that the STM32 and BBG share a common ground.

Required connections:

```text
STM32 SCL → BBG P9.19
STM32 SDA → BBG P9.20
STM32 GND → BBG GND
```

Verify that the Linux I2C device exists:

```bash
ls -l /dev/i2c-2
```

---

## Service Does Not Start After Boot

Check whether it is enabled:

```bash
systemctl is-enabled bbg-client.service
```

Enable it if necessary:

```bash
systemctl enable bbg-client.service
```

Inspect the current status:

```bash
systemctl status bbg-client.service
```

---

# Final Runtime Model

```text
systemd
└── bbg-client.service
    └── /root/bbg_client_arm
        ├── TCP process
        └── I2C process
```

The service:

- starts automatically during boot;
- reads GPS coordinates from STM32 over I2C;
- transfers parking events through the internal pipe;
- sends requests to the TCP Server;
- writes separate TCP and I2C log files;
- restarts automatically after unexpected failures.
