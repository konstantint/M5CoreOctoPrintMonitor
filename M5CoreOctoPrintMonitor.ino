#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h> 
#include "config.h"

// App Modes
enum AppMode { MODE_VIEW, MODE_CONFIRM_PAUSE, MODE_CONFIRM_RESUME, MODE_CONFIRM_CANCEL };
AppMode currentMode = MODE_VIEW;

// Buffers & Objects
uint8_t jpg_buffer[MAX_IMAGE_SIZE];
TFT_eSprite topSprite = TFT_eSprite(&M5.Lcd);

// State Variables
int current_cam_idx = 0; 
float val_bed = 0.0;
float val_tool = 0.0;
float val_progress = 0.0;
long  val_seconds_left = -1;
bool  is_printing = false;
bool  data_received = false;

unsigned long lastSensorUpdate = 0;

// -------------------------------------------------------------------------
// 1. CLIPPED DECODER
// -------------------------------------------------------------------------
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap){
   // Clip Bottom
   if (y >= CAM_END_Y) return 1;
   if ((y + h) > CAM_END_Y) h = CAM_END_Y - y;

   // Clip Top (Handle Negative Y)
   if ((y + h) < CAM_START_Y) return 1;
   
   if (y < CAM_START_Y) {
     int linesToSkip = CAM_START_Y - y;
     bitmap += (linesToSkip * w);
     h -= linesToSkip;
     y = CAM_START_Y;
   }

   M5.Lcd.pushImage(x, y, w, h, bitmap);
   return 1;
}

void beep() {
  M5.Speaker.setVolume(1); 
  M5.Speaker.tone(1000, 200); 
  delay(200);
  M5.Speaker.mute();
}

// Forward declarations
void updateSensors();
void triggerHAButton(String entity_id);
void drawTopBar();
void drawBottomBarLabels();

void setup() {
  M5.begin();
  M5.Power.begin();
  
  M5.Speaker.begin();
  M5.Speaker.mute();
  pinMode(25, OUTPUT);
  digitalWrite(25, LOW); 

  M5.Lcd.setBrightness(200);
  M5.Lcd.fillScreen(BLACK);
  
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println("Connecting WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  
  topSprite.setColorDepth(8);
  topSprite.createSprite(SCREEN_W, TOP_BAR_H);
  
  TJpgDec.setJpgScale(1); 
  TJpgDec.setSwapBytes(true); 
  TJpgDec.setCallback(tft_output);
  
  M5.Lcd.fillScreen(BLACK);
  
  // Force load
  updateSensors();
  drawTopBar();
  drawBottomBarLabels();
}

void updateSensors() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(HA_BASE_URL + "/api/template");
  http.addHeader("Authorization", "Bearer " + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  String query = "{\"template\": \"{";
  query += "\\\"bed\\\": {{ states('" + ENT_BED + "') | float(0) }},";
  query += "\\\"tool\\\": {{ states('" + ENT_TOOL + "') | float(0) }},";
  query += "\\\"prog\\\": {{ states('" + ENT_PROGRESS + "') | float(0) }},";
  query += "\\\"left\\\": {{ -1 if states('" + ENT_FINISH + "') in ['unknown', 'unavailable', 'None'] else (as_timestamp(states('" + ENT_FINISH + "')) - as_timestamp(now())) | int(0) }},";
  query += "\\\"printing\\\": {{ is_state('" + ENT_PRINTING + "', 'on') | tojson }}";
  query += "}\"}";

  int httpCode = http.POST(query);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      val_bed = doc["bed"];
      val_tool = doc["tool"];
      val_progress = doc["prog"];
      val_seconds_left = doc["left"];
      is_printing = doc["printing"];
      data_received = true;
    }
  }
  http.end();
}

void drawTopBar() {
  topSprite.fillSprite(0x10A2); // Dark Gray
  topSprite.setTextColor(WHITE, 0x10A2);
  topSprite.setTextSize(2);

  String timeStr;
  if (val_seconds_left == -1) {
     if (val_progress > 0 && val_progress < 100) timeStr = "Printing...";
     else if (val_progress >= 100) timeStr = "Done";
     else timeStr = "Idle";
  }
  else if (val_seconds_left <= 0) timeStr = "Done";
  else {
    int hours = val_seconds_left / 3600;
    int mins = (val_seconds_left % 3600) / 60;
    timeStr = "ETA:" + String(hours) + "h" + String(mins) + "m";
  }

  String statusLine = "Temp:" + String((int)val_bed) + "|" + String((int)val_tool) + " " + timeStr;
  
  int textWidth = M5.Lcd.textWidth(statusLine);
  int textX = (SCREEN_W - textWidth) / 2;
  topSprite.setCursor(textX, 4);
  topSprite.print(statusLine);

  int barY = 28;
  int barH = 8;
  int barW = (int)((val_progress / 100.0) * 300);
  
  topSprite.fillRect(10, barY, 300, barH, BLACK);
  topSprite.fillRect(10, barY, barW, barH, GREEN);

  topSprite.pushSprite(0, 0);
}

void drawBottomBarLabels() {
  M5.Lcd.fillRect(0, CAM_END_Y, SCREEN_W, BTM_BAR_H, BLACK);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);

  M5.Lcd.setCursor(19+10, CAM_END_Y + 5);
  if (is_printing) M5.Lcd.print("Pause");
  else M5.Lcd.print("Resume");

  M5.Lcd.setCursor(106 + 19, CAM_END_Y + 5);
  M5.Lcd.print("Cancel");

  M5.Lcd.setCursor(213 + 19, CAM_END_Y + 5);
  M5.Lcd.print("Camera");
  
  M5.Lcd.drawLine(106, CAM_END_Y, 106, SCREEN_H, 0x3186); 
  M5.Lcd.drawLine(213, CAM_END_Y, 213, SCREEN_H, 0x3186);
  M5.Lcd.drawLine(0, CAM_END_Y, SCREEN_W, CAM_END_Y, 0x3186);
}

void drawConfirmationScreen(String action) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  
  M5.Lcd.setCursor(40, 80);
  M5.Lcd.print("Are you sure you");
  M5.Lcd.setCursor(60, 105);
  M5.Lcd.printf("want to %s?", action.c_str());
  
  M5.Lcd.setCursor(45, 215);
  M5.Lcd.setTextColor(GREEN, BLACK);
  M5.Lcd.print("YES");
  
  M5.Lcd.setCursor(250, 215);
  M5.Lcd.setTextColor(RED, BLACK);
  M5.Lcd.print("NO");
}

void redrawViewInterface() {
  M5.Lcd.fillScreen(BLACK);
  drawBottomBarLabels();
  updateSensors();
  drawTopBar();
}

void loop() {
  M5.update(); 

  if (currentMode == MODE_VIEW) {
    
    if (M5.BtnA.wasPressed()) {
      if (is_printing) {
        currentMode = MODE_CONFIRM_PAUSE;
        drawConfirmationScreen("PAUSE");
      } else {
        currentMode = MODE_CONFIRM_RESUME;
        drawConfirmationScreen("RESUME");
      }
      return; 
    }
    if (M5.BtnB.wasPressed()) {
      currentMode = MODE_CONFIRM_CANCEL;
      drawConfirmationScreen("CANCEL");
      return; 
    }
    if (M5.BtnC.wasPressed()) {
      current_cam_idx = (current_cam_idx + 1) % 2;
      M5.Lcd.fillRect(0, CAM_START_Y, SCREEN_W, CAM_END_Y - CAM_START_Y, BLACK);
      M5.Lcd.setCursor(100, 100);
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.print("Switching...");
    }

    unsigned long now = millis();
    if (now - lastSensorUpdate > SENSOR_INTERVAL) {
      updateSensors();
      drawTopBar();
      lastSensorUpdate = now;
    }

    if (WiFi.status() == WL_CONNECTED) {
      String active_cam = (current_cam_idx == 0) ? ENT_CAM_1 : ENT_CAM_2;
      
      HTTPClient http;
      http.begin(HA_BASE_URL + "/api/camera_proxy/" + active_cam);
      http.addHeader("Authorization", "Bearer " + HA_TOKEN);
      http.setReuse(true);

      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        WiFiClient *stream = http.getStreamPtr();
        int len = http.getSize();

        if (len > 0 && len < MAX_IMAGE_SIZE) {
          int bytesRead = 0;
          while (http.connected() && (bytesRead < len)) {
            int available = stream->available();
            if (available) {
              int c = stream->readBytes(jpg_buffer + bytesRead, available);
              bytesRead += c;
            }
            delay(1); 
          }
          if (current_cam_idx == 0) {
            TJpgDec.drawJpg(0, IMG_DRAW_Y_CAM1, jpg_buffer, bytesRead);
          }
          else {
            TJpgDec.drawJpg(0, IMG_DRAW_Y_CAM2, jpg_buffer, bytesRead);
          }
        }
      }
      http.end();
    }
  }

  else if (currentMode == MODE_CONFIRM_PAUSE || currentMode == MODE_CONFIRM_CANCEL || currentMode == MODE_CONFIRM_RESUME) {
    if (M5.BtnA.wasPressed()) { 
      beep();
      
      if (currentMode == MODE_CONFIRM_PAUSE) {
        triggerHAButton(ENT_BTN_PAUSE);
        is_printing = false;
      }
      else if (currentMode == MODE_CONFIRM_RESUME) {
        triggerHAButton(ENT_BTN_RESUME);
        is_printing = true;
      }
      else if (currentMode == MODE_CONFIRM_CANCEL) {
        triggerHAButton(ENT_BTN_CANCEL);
        is_printing = false;
      }
 
      currentMode = MODE_VIEW;
      redrawViewInterface();
    }
    if (M5.BtnC.wasPressed()) { 
      currentMode = MODE_VIEW;
      redrawViewInterface();
    }
    delay(50); 
  }
}

void triggerHAButton(String entity_id) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(HA_BASE_URL + "/api/services/button/press");
  http.addHeader("Authorization", "Bearer " + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"entity_id\": \"" + entity_id + "\"}";
  http.POST(payload);
  http.end();
}