# RFID Based Smart Door

## Overview

The RFID Based Smart Door is an ESP32-powered access control and automation project designed to provide secure and intelligent door operation. The system integrates RFID authentication, motion sensing, and an automatic sliding door mechanism to deliver a modern access control solution.

This project demonstrates the application of embedded systems, automation, motor control, and smart home technologies in a real-world scenario.

---

## Features

- RFID-based authentication using MFRC522
- Automatic sliding door operation
- PIR-based motion detection
- Solenoid lock control
- Audio feedback using JQ6500 Voice Module
- EEPROM state retention after power loss
- OTA (Over-The-Air) firmware updates
- Door position monitoring using limit switches
- BTS7960 motor driver control
- Non-blocking firmware architecture
- Finite State Machine (FSM) based operation

---

## Hardware Components

- ESP32 Dev Module
- MFRC522 RFID Reader
- BTS7960 Motor Driver
- DC Gear Motor
- Solenoid Lock
- PIR Sensor
- JQ6500 Voice Module
- Limit Switches / Proximity Sensors
- RFID Cards/Tags
- Power Supply Unit

---

## System Working

1. The user authenticates using a registered RFID card.
2. The ESP32 verifies the RFID credentials.
3. The solenoid lock is released.
4. The system becomes active for automatic door operation.
5. The PIR sensor detects motion and triggers door movement.
6. The BTS7960 motor driver controls the sliding door mechanism.
7. Limit switches monitor fully open and fully closed positions.
8. Audio feedback is provided through the JQ6500 module.
9. System status is stored in EEPROM to maintain operation after power interruptions.
10. OTA updates allow wireless firmware upgrades.

---

## Software Features

- RFID Authentication
- Automatic Door Control
- Finite State Machine (FSM) Architecture
- EEPROM Data Storage
- OTA Firmware Updates
- Motor Speed Control
- Event-Driven Programming

---

## Repository Structure

```text
RFID-Based-Smart-Door/
│
├── Firmware/
│   └── ESP32_Smart_Door.ino
│
├── Hardware/
│   ├── Circuit_Diagram.pdf
│   └── Wiring_Diagram.png
│
├── Documentation/
│   ├── Project_Report.pdf
│   └── User_Manual.pdf
│
├── Simulation/
│   └── RFID_Based_Smart_Door_Simulator.html
│
├── Images/
│
└── README.md
```

---

## Future Improvements

- Mobile application integration
- Cloud connectivity and monitoring
- Face recognition-based access control
- Fingerprint authentication
- Event logging and analytics
- Web dashboard for remote management

---

## Author

Pratik Pisal

Electronics and Telecommunication Engineer  
Diploma in Mechatronics

### Areas of Interest

- Embedded Systems
- Automation
- Robotics
- Internet of Things (IoT)
- Industrial Control Systems
- Smart Home Technologies
