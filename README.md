# M5Stack Gray 3D Printing Mini-Monitor

A small remote monitor for a 3D printer based on an **M5Stack Gray** (which should be compatible with the newer M5Stack Core models).
The implementation relies on OctoPrint's HomeAssistant integration to view the downscaled camera image and read the sensor data.

![Mini-monitor](device.jpg?raw=true "Mini-monitor")

## Features

*   **Live Video Feed:** Streams MJPEG snapshots from Home Assistant.
*   **Split-Screen UI:**
    *   **Top:** Status Bar (Bed/Nozzle Temp, Progress Bar, ETA).
    *   **Middle:** Camera Feed.
    *   **Bottom:** Button Labels.
*   **Multi-Camera Support:** Toggle between two different camera entities.
*   **Flicker-Free Updates:** Uses Sprite buffering for smooth UI refreshing.
*   **Interactivity:**
    *   **Button A:** Pause Print (with "Are you sure?" confirmation).
    *   **Button B:** Cancel Print (with "Are you sure?" confirmation).
    *   **Button C:** Switch Camera.

## Prerequisites

### Hardware
*   Implemented and tested on M5Stack Gray (ESP32-D0WDQ6-V3), but would probably work on M5Core models as well.

### Software
*   [arduino-cli](https://arduino.github.io/arduino-cli/latest/) or Arduino IDE.
*   Home Assistant instance with OctoPrint integration configured.

## Home Assistant Configuration

**Crucial Step:** The M5Stack Gray has limited RAM. You **must** create a downscaled proxy camera in Home Assistant, or the device will crash trying to decode full HD images.

Add this to your Home Assistant `configuration.yaml`:

```yaml
camera:
  - platform: proxy
    entity_id: camera.octoprint_camera # Your actual high-res source camera
    name: "Octoprint Camera Downscaled"
    max_image_width: 320
    max_stream_width: 320
    image_quality: 75
```

*Restart Home Assistant after adding this.*

## Installation & Flashing

This project uses a self-contained `sketch.yaml` to manage board cores and library dependencies.

### 1. Clone and Configure
1.  Clone this repository.
2.  Copy the example config:
    ```bash
    cp config.example.h config.h
    ```
3.  Open `config.h` and enter your:
    *   WiFi Credentials.
    *   Home Assistant URL (e.g., `http://192.168.1.100:8123`).
    *   Home Assistant **Long-Lived Access Token** (Create this in HA User Profile -> Security).
    *   Entity IDs for your cameras and OctoPrint sensors.

### 2. Compile and Upload (Using Arduino CLI)

This project includes a `sketch.yaml` that defines the exact M5Stack platform and libraries required.

1.  **Initialize the Core & Libraries:**
    Run this command in the project directory. It downloads the M5Stack core and required libraries (`M5Stack`, `TJpg_Decoder`, `ArduinoJson`) automatically based on `sketch.yaml`.
    ```bash
    arduino-cli core install
    arduino-cli lib install
    ```

2.  **Compile:**
    ```bash
    arduino-cli compile .
    ```

3.  **Upload:**
    Connect your M5Stack via USB and identify the port (e.g., `/dev/ttyUSB0` or `COM3`).
    ```bash
    arduino-cli upload -p /dev/ttyUSB0
    ```
## Usage

*   **Boot:** The device will connect to WiFi, fetch the initial sensor data, and start streaming.
*   **Button A (Left):** Request to **PAUSE** the print. Shows a confirmation screen.
*   **Button B (Center):** Request to **CANCEL** the print. Shows a confirmation screen.
*   **Button C (Right):** Toggle between Camera 1 and Camera 2.
*   **Status Bar:**
    *   **B:** Bed Temperature.
    *   **N:** Nozzle Temperature.
    *   **ETA:** Estimated time remaining (Calculated from HA timestamp).
    *   **Idle/Heating/Printing:** Status text updates based on printer state.

## Troubleshooting

*   **Screen flashes white/black:** Check the Serial Monitor. If you see `Image too large`, ensure you are using the **Downscaled** camera entity in `config.h`.
*   **HTTP Fail 404:** Check your Entity IDs in `config.h`.
*   **HTTP Fail 401:** Check your Long-Lived Access Token.
*   **Upload Fails:** Ensure your USB cable supports data (not just power) and that no other software (like Cura or Arduino IDE) is hogging the serial port.
