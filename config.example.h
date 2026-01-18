#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================= WIFI SETTINGS =================
const char* WIFI_SSID     = "YOUR WIFI SSID";
const char* WIFI_PASSWORD = "YOUR WIFI PASSWORD";

// ================= HOME ASSISTANT =================
// IP address of Home Assistant (No trailing slash)
const String HA_BASE_URL = "http://homeassistant.local:8123";

// Long-Lived Access Token
const String HA_TOKEN = "<Get this from HA user profile -> Security>";

// ================= ENTITY CONFIGURATION =================
// Camera Entities
const String ENT_CAM_1    = "camera.octoprint_camera_downscaled";
const String ENT_CAM_2    = "camera.secondary_camera_downscaled";

// Sensor Entities
const String ENT_PROGRESS = "sensor.octoprint_job_percentage";
const String ENT_FINISH   = "sensor.octoprint_estimated_finish_time";
const String ENT_BED      = "sensor.octoprint_actual_bed_temp";
const String ENT_TOOL     = "sensor.octoprint_actual_tool0_temp";
const String ENT_PRINTING = "binary_sensor.octoprint_printing";

// Button Entities
const String ENT_BTN_PAUSE  = "button.octoprint_pause_job";
const String ENT_BTN_RESUME = "button.octoprint_resume_job";
const String ENT_BTN_CANCEL = "button.octoprint_stop_job";

// ================= LAYOUT CONFIGURATION =================
// Dimensions
#define SCREEN_W      320
#define SCREEN_H      240
#define TOP_BAR_H     40
#define BTM_BAR_H     25 

// Calculated Zones (Do not change unless you know what you are doing)
#define CAM_START_Y   TOP_BAR_H             // 40
#define CAM_END_Y     (SCREEN_H - BTM_BAR_H)  // 215

// Image Offset Calculation
// We want the BOTTOM of the 240px image for the first camera to align with CAM_END_Y.
// Offset = Limit (215) - Image Height (240) = -25
#define IMG_DRAW_Y_CAM1    (CAM_END_Y - 240)
// For the second camera we draw the top part of the camera right under the top bar.
#define IMG_DRAW_Y_CAM2   TOP_BAR_H

// System Settings
#define MAX_IMAGE_SIZE 30000
#define SENSOR_INTERVAL 5000 // Milliseconds

#endif