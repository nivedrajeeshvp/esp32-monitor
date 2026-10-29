# Nexus Monitor — ESP32 Gas & Environmental Monitoring System

An ESP32-based gas and environmental monitoring system integrating MQ-series gas sensors, an SHT31 temperature and humidity sensor, embedded firmware, and a web-based monitoring dashboard.

## Overview

Nexus Monitor is an embedded IoT project designed to collect gas-sensor readings and environmental measurements using an ESP32 development board. The system combines sensor interfacing, Wi-Fi connectivity, an HTTP API, and a web interface to present monitoring data.

The project also includes custom schematic and PCB design work using KiCad.

## Features

- **Gas monitoring:** Interface with MQ-3, MQ-137, and MQ-138 gas sensors.
- **Environmental monitoring:** Measure temperature and relative humidity using the SHT31 sensor.
- **ADC-based acquisition:** Read analog gas-sensor signals through the ESP32 ADC.
- **I²C communication:** Communicate with the SHT31 sensor.
- **Wi-Fi connectivity:** Configure the ESP32 for wireless access point operation.
- **HTTP API:** Make sensor readings available to the web dashboard.
- **Web dashboard:** Display monitoring information through HTML, CSS, and JavaScript.
- **LittleFS:** Store web assets in the ESP32 flash filesystem.
- **PCB design:** Develop circuit schematics and PCB layouts using KiCad.

## Hardware Components

| Component | Purpose |
|---|---|
| ESP32 DevKit | Main microcontroller and Wi-Fi connectivity |
| MQ-3 gas sensor | Gas-sensing input |
| MQ-137 gas sensor | Gas-sensing input |
| MQ-138 gas sensor | Gas-sensing input |
| SHT31 sensor | Temperature and relative humidity measurement |
| Custom PCB | Sensor and microcontroller circuit integration |

*Confirm the exact sensor modules, board revision, and circuit connections against your hardware before fabrication or deployment.*

## Software and Technologies

- **Firmware:** C/C++, Arduino framework
- **Microcontroller platform:** ESP32
- **Build environment:** PlatformIO
- **Sensor libraries:** Adafruit SHT31 Library, Adafruit BusIO
- **Communication:** I²C, Wi-Fi, HTTP
- **Web technologies:** HTML5, CSS3, JavaScript
- **Filesystem:** LittleFS
- **PCB design:** KiCad
- **Version control:** Git and GitHub

## Repository Structure

```text
esp32-monitor/
├── src/
│   └── main.cpp
├── include/
├── lib/
├── test/
├── data/
│   ├── app.js
│   ├── index.html
│   └── style.css
├── app.js
├── index.html
├── style.css
├── platformio.ini
├── .gitignore
└── README.md
```

The `data/` directory contains the web assets intended for the ESP32 LittleFS filesystem. The root-level web files are retained for the existing web deployment.

If KiCad design files are added, they can be organized under a `hardware/` directory.

## Getting Started

### Prerequisites

Install the following software:

1. [Visual Studio Code](https://code.visualstudio.com/)
2. [PlatformIO IDE](https://platformio.org/install/ide?install=vscode)
3. USB drivers appropriate for your ESP32 development board
4. Git, if you intend to clone and modify the repository

### Clone the Repository

```bash
git clone https://github.com/nivedrajeeshvp/esp32-monitor.git
cd esp32-monitor
```

### Configure the Firmware

The current `platformio.ini` configuration uses the ESP32 DevKit board and Arduino framework.

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

monitor_speed = 115200
board_build.filesystem = littlefs

lib_deps =
    adafruit/Adafruit SHT31 Library
    adafruit/Adafruit BusIO
```

Open the repository in VS Code with PlatformIO installed.

### Build and Upload Firmware

Connect your ESP32 development board and select the correct serial port.

Build the firmware:

```bash
pio run
```

Upload the firmware:

```bash
pio run --target upload
```

### Upload LittleFS Assets

Place the web assets intended for the ESP32 filesystem in the `data/` directory.

Build and upload the filesystem image:

```bash
pio run --target buildfs
pio run --target uploadfs
```

**Note:** Uploading a LittleFS image replaces the existing filesystem contents. Keep a backup of any files stored on the device that you need to preserve.

### Monitor Serial Output

```bash
pio device monitor
```

The configured serial baud rate is 115200.

## System Architecture

```text
MQ-3 ──────┐
MQ-137 ────┼──> ESP32 ADC ──────┐
MQ-138 ────┘                    │
                                ├──> ESP32 Firmware
SHT31 ──────── I²C ─────────────┘          │
                                           ▼
                                      Wi-Fi / HTTP
                                           │
                                           ▼
                                    Web Dashboard
```

The firmware acquires sensor measurements and provides data to the web interface through the configured wireless and HTTP communication mechanisms.

## Web Dashboard

The project includes a web interface built using HTML, CSS, and JavaScript.

The web assets can be used by the existing web deployment and, where configured, stored in ESP32 flash using LittleFS.

For ESP32-hosted operation, the firmware must serve the relevant assets and expose the HTTP endpoints expected by the dashboard.

## PCB Design

The project includes KiCad-based electronic circuit design work for integrating the ESP32 and sensor interfaces.

Planned hardware documentation includes:

- Schematic files
- PCB layout files
- Bill of materials (BOM)
- Pin mapping and wiring diagrams
- Fabrication outputs, when available

Add the corresponding KiCad files and documentation to the repository as they become available.

## Calibration and Safety

- MQ-series sensors can require appropriate preheating, calibration, and environmental compensation.
- Raw ADC values should not be presented as gas concentrations in ppm unless a validated calibration method supports that conversion.
- Confirm sensor-module output voltages are compatible with the ESP32 ADC input limits.
- Follow the relevant sensor datasheets and electrical safety guidelines.

## Project Status

**Development status:** In progress.

The repository contains ESP32 firmware configuration and web-dashboard files. Complete hardware integration, sensor calibration, and end-to-end validation should be documented once testing has been performed.

## Future Improvements

- Add calibrated gas-concentration estimation where supported.
- Improve sensor calibration and measurement filtering.
- Add historical measurement logging and visualization.
- Expand PCB documentation and component specifications.
- Add screenshots, wiring diagrams, and hardware test results.
- Add automated firmware build checks.

## Author

**Nived Rajeesh V P**

- GitHub: [@nivedrajeeshvp](https://github.com/nivedrajeeshvp)
- Project repository: [esp32-monitor](https://github.com/nivedrajeeshvp/esp32-monitor)

## License

A license has not yet been specified. Add an appropriate open-source license before granting others permission to reuse, modify, or distribute the project.
