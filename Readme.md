# ESP32 LED Tester

A simple project for testing and controlling **SK6812 LEDs** with an **ESP32** microcontroller using [PlatformIO](https://platformio.org/). This repository provides a minimal setup to quickly verify LED connections. It also allows to control the LEDs individually using a Bluetooth (BLE) based WebApp.

To individually control the LEDs use the following WebApp (Requires a Chromium based Browser). This project is based on my [Web-BLE library](https://registry.platformio.org/libraries/tirus/BLE%20Remote).

- URL: [https://tirus.mine.nu/ble/](https://tirus.mine.nu/ble/)
- Mirror: [https://tirus42.github.io/ble/](https://tirus42.github.io/ble/)

---

## Features
- Simple test animation for up to 100 LEDs on startup
- Control LEDs connected to the ESP32 GPIO pins using a BLE based Web interface.

---

## Requirements
### Hardware
- ESP32 development board (pre-configured for ESP32-C3)
- One or more SK6812 LEDs
- Optional: Breadboard and jumper wires
- Optional: Chromium based Browser or Android Phone

---

## Minimal setup
- Create the PlatformIO build structure inside the project folder
  - ```pio project init .```
- Flash the program on a ESP32-C3 using PlatformIO
  - ```pio run -t upload```
- Connect the SK6812 LED with its Data-In pin to Pin 0 or Pin 1 of the ESP32-C3
  - Pin 0 for RGB+W LEDs, Pin 1 for RGB-only LEDs
- Connect a 5V power source to the ESP32-C3 and the LED
- Now you should see a rainbow animation using all color channels

---


