# Requirements Specification: M5Stack OctoPrint Viewer

## 1. Project Overview
The goal is to develop a firmware application for the **M5Stack Gray** ESP32 development board. The device will function as a remote dashboard for a 3D printer managed by OctoPrint, utilizing **Home Assistant** as the middleware API.

The device must display a live low-latency video feed, real-time printing statistics (temperatures, progress, ETA), and provide physical controls to pause or cancel the print job.

## 2. Hardware Environment
*   **Target Device:** M5Stack Gray (ESP32-D0WDQ6-V3).
*   **Display:** ILI9341 320x240 Color TFT LCD.
*   **Memory Constraints:** No PSRAM available. Usable Heap is <100KB after WiFi initialization.
*   **Input:** 3 Physical Buttons (A, B, C).
*   **Audio:** Internal Speaker (DAC via GPIO 25).
*   **Power Management:** IP5306 Chip. Requires I2C keep-alive command to prevent auto-shutdown on battery.

## 3. External System Interfaces (Home Assistant)
The device does not communicate with OctoPrint directly. It communicates exclusively with the Home Assistant REST API.

### 3.1 Authentication
*   **Method:** HTTP Bearer Token.
*   **Credential:** Long-Lived Access Token provided in configuration.

### 3.2 Camera Feed API
*   **Endpoint:** `GET /api/camera_proxy/{entity_id}`
*   **Format:** MJPEG/JPEG binary stream.
*   **Constraint:** The standard 1080p/720p streams are too large for the M5Stack's RAM.
*   **Requirement:** Home Assistant must expose a `platform: proxy` camera entity configured to downscale the image to a maximum width of **320px** and a file size < **30KB**.

### 3.3 Sensor Data API
*   **Endpoint:** `POST /api/template`
*   **Method:** Server-side Jinja2 Template rendering.
*   **Reasoning:** To reduce network overhead, multiple sensor states must be fetched in a single HTTP request.
*   **Required Data Fields:**
    1.  **Bed Temp:** `sensor.octoprint_actual_bed_temp`
    2.  **Tool Temp:** `sensor.octoprint_actual_tool0_temp`
    3.  **Progress:** `sensor.octoprint_job_percentage`
    4.  **Finish Time:** `sensor.octoprint_estimated_finish_time` (Must handle 'unknown' states).
    5.  **Printing Status:** `binary_sensor.octoprint_printing` (Used to toggle Pause/Resume).

## 4. Functional Requirements

### 4.1 layout & Display Zones
The 320x240 screen is strictly divided into three vertical zones to prevent flickering and overwriting.

1.  **Top Zone (Y: 0 - 40):** Status Bar.
    *   **Content:** Bed Temp, Nozzle Temp, ETA/Status String, Progress Bar.
    *   **Rendering:** Buffered via Sprite (Double buffering) to ensure text and bar update simultaneously without flicker.
2.  **Middle Zone (Y: 40 - 215):** Camera Viewport.
    *   **Content:** Cropped video feed (Top or Bottom aligned).
    *   **Rendering:** Direct push to display memory (No buffering due to RAM limits). Pixels must be strictly clipped to stay within Y=40 and Y=215.
3.  **Bottom Zone (Y: 215 - 240):** Button Labels.
    *   **Content:** Labels aligned with physical buttons.
        *   **Button A:** "Pause" (if printing) OR "Resume" (if not printing).
        *   **Button B:** "Cancel".
        *   **Button C:** "Camera".
    *   **Rendering:** Text redrawn upon mode change or status change (Printing/Not Printing).

### 4.2 Application Modes (State Machine)
The system operates in two distinct modes:

#### Mode A: Viewer (Default)
*   **Action:** Continuously fetches/decodes JPEG frames and updates sensor data every 5 seconds.
*   **Button A:** Switches to **Confirmation Mode** (Action: PAUSE or RESUME depending on status).
*   **Button B:** Switches to **Confirmation Mode** (Action: CANCEL).
*   **Button C:** Toggles the `current_camera_entity` between Camera 1 and Camera 2. Displays a temporary "Switching..." overlay.

#### Mode B: Confirmation
*   **Triggered by:** Pressing Pause or Cancel in Viewer Mode.
*   **Display:** Clears screen to black. Shows "Are you sure you want to {ACTION}?"
*   **Network:** Stops camera streaming to save resources.
*   **Button A (Yes):** plays an audible beep, (Future: triggers API call), and returns to Viewer Mode.
*   **Button C (No):** Returns to Viewer Mode without action.

### 4.3 Sensor Logic
The "ETA" display must handle specific logic states based on sensor data:
1.  **Idle:** Finish Time is unknown/unavailable AND Progress is 0%.
2.  **Printing...:** Finish Time is unknown BUT Progress > 0%.
3.  **ETA: XhYYm:** Finish Time is valid. (Calculated as `FinishTimestamp - CurrentTimestamp`).
4.  **Done:** Progress is 100% or Finish Time <= 0.

## 5. Non-Functional Requirements (Architecture)

### 5.1 Memory Management
*   **JPEG Buffer:** A static buffer of **30,000 bytes** must be allocated. This is the hard limit for a single video frame. If a downloaded frame exceeds this, it must be discarded to prevent heap overflow.
*   **Sprite Buffer:** The Top Status bar sprite (320x40 @ 8-bit color) consumes ~12.8KB RAM. This is acceptable.

### 5.2 Video Decoding Strategy
*   **Library:** `TJpg_Decoder`.
*   **Method:**
    1.  Download compressed JPEG into the static 30KB buffer.
    2.  Invoke decoder.
    3.  **Clipping Callback:** The decoder callback `tft_output` must implement logic to discard any 16x16 pixel blocks that fall *outside* the Middle Zone (Y < 40 or Y > 215) to preserve the UI overlays.
    4.  **Vertical Offset:** The draw coordinate Y-offset must be configurable per camera to allow displaying either the top or bottom portion of the video feed, as the full height exceeds the viewport.

### 5.3 Build System
*   **Toolchain:** Arduino CLI.
*   **Configuration:** All secrets (WiFi/Tokens) must be isolated in a `config.h` file.
*   **Dependency Management:** A `sketch.yaml` file must define specific versions of `M5Stack`, `TJpg_Decoder`, and `ArduinoJson` to ensure reproducibility.

## 6. Implementation Checklist

1.  **M5Stack Initialization:**
    *   `M5.begin()`
    *   `M5.Power.begin()` (Vital for IP5306)
    *   Mute Speaker (DAC GPIO 25 set to 0).
2.  **WiFi Connection:** Blocking loop until connected.
3.  **State Loop:**
    *   `M5.update()` for button polling.
    *   `millis()` timer for Sensor Refresh (5000ms).
    *   `HTTPClient` for Camera Stream (As fast as possible).
4.  **Template Query:**
    *   JSON Payload: `{"template": "{ ... jinja2 logic ... }"}`.
5.  **UI Drawing:**
    *   Use `pushSprite` for the top bar.
    *   Use `pushImage` inside the JPEG callback for the video.