#include <Arduino.h>
#line 1 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>

// ================= WIFI CONFIG =================
const char *ssid = "HitchHiker";
const char *password = "noc@pkcl";

// Static IP Configuration
IPAddress local_IP(10, 81, 100, 72);
IPAddress gateway(10, 81, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

// ================= WEB SERVER =================
WebServer server(80);

// ================= HARDWARE PINS (CYD) =================
#define RELAY_PIN 22
#define GREEN_LED 17
#define BACKLIGHT_PIN 21

// ================= DISPLAY =================
int currentRotation = 6; // 6 = Normal un-mirrored Portrait (240x320)
TFT_eSPI tft;

// ================= SYSTEM STATE =================
unsigned long actionStart = 0;
bool activeMode = false;
String currentEmpId = "";
String currentName = "";

// ================= RELAY CONTROL (INVERTED) =================
// LOW  = OPEN (Unlock Door)
// HIGH = CLOSE (Lock Door / Safe State)

#line 39 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void openDoor();
#line 44 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void closeDoor();
#line 51 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void drawIdleScreen();
#line 105 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void drawAccessCard(String name, String empId);
#line 167 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void enterIdle();
#line 177 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
String getNameFromRequest();
#line 206 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
String getIdFromRequest();
#line 237 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void processUnlockRequest(const char *source);
#line 292 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void handleOn();
#line 294 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void handleSilent();
#line 296 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void handleRotation();
#line 314 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void handleInvert();
#line 326 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void setup();
#line 400 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void loop();
#line 39 "/Users/psl/hanif_3.0/Door-lock-ai-main/Door-lock-ai-main.ino"
void openDoor() {
  digitalWrite(RELAY_PIN, LOW);  // Unlock
  digitalWrite(GREEN_LED, HIGH); // Green LED ON
}

void closeDoor() {
  digitalWrite(RELAY_PIN, HIGH); // Lock
  digitalWrite(GREEN_LED, LOW);  // Green LED OFF
}

// ================= DRAW STANDBY / IDLE SCREEN (100% FULL SCREEN)
// =================
void drawIdleScreen() {
  tft.setRotation(currentRotation); // Portrait (240x320)
  tft.fillScreen(TFT_WHITE);         // Pure clean white full-screen background

  // 1. Top Header Banner: Pakiza Software
  tft.fillRect(0, 0, 240, 62, 0x0110); // Deep Navy Blue header (0,0 to 240,62)
  tft.drawFastHLine(0, 61, 240, 0x039F); // Bright cyan accent bar
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(2);
  tft.drawCentreString("PAKIZA", 120, 12, 2);
  tft.setTextColor(0x5E3F, 0x0110); // Soft cyan
  tft.setTextSize(1);
  tft.drawCentreString("SOFTWARE LTD", 120, 38, 1);

  // 2. Center Face Recognition Standby Card
  tft.fillRoundRect(12, 75, 216, 185, 10, 0xF7BE); // Light clean gray card
  tft.drawRoundRect(12, 75, 216, 185, 10, 0xC618); // Crisp outline

  // Camera / Scan target bracket
  int cx = 120, cy = 138, r = 36;
  tft.drawCircle(cx, cy, r, 0x0110);
  tft.drawCircle(cx, cy, r - 1, 0x0110);
  tft.fillCircle(cx, cy, 6, TFT_GREEN); // Green target dot

  // Corner brackets for facial scan feel
  tft.drawFastHLine(cx - 45, cy - 45, 18, 0x0110);
  tft.drawFastVLine(cx - 45, cy - 45, 18, 0x0110);
  tft.drawFastHLine(cx + 27, cy - 45, 18, 0x0110);
  tft.drawFastVLine(cx + 45, cy - 45, 18, 0x0110);
  tft.drawFastHLine(cx - 45, cy + 45, 18, 0x0110);
  tft.drawFastVLine(cx - 45, cy + 27, 18, 0x0110);
  tft.drawFastHLine(cx + 27, cy + 45, 18, 0x0110);
  tft.drawFastVLine(cx + 45, cy + 27, 18, 0x0110);

  // Standby Prompt text
  tft.setTextColor(0x0110, 0xF7BE);
  tft.setTextSize(2);
  tft.drawCentreString("READY TO SCAN", 120, 192, 2);
  tft.setTextColor(0x7BEF, 0xF7BE); // Soft slate
  tft.setTextSize(1);
  tft.drawCentreString("Please look at camera", 120, 222, 1);

  // 3. Footer Banner: Online Status & IP
  tft.fillRoundRect(12, 272, 216, 36, 6, 0x0110);
  tft.fillCircle(26, 290, 4, TFT_GREEN); // Green online LED
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(1);
  tft.drawString("SYSTEM ONLINE", 38, 285, 1);
  tft.setTextColor(0x5E3F, 0x0110);
  tft.drawRightString(WiFi.localIP().toString(), 216, 285, 1);
}

// ================= DRAW ACCESS GRANTED / EMPLOYEE CARD (100% FULL SCREEN)
// =================
void drawAccessCard(String name, String empId) {
  tft.setRotation(currentRotation); // Portrait (240x320)
  tft.fillScreen(TFT_WHITE);         // Pure clean background

  // 1. Top Header Banner: Pakiza Software
  tft.fillRect(0, 0, 240, 62, 0x0110);
  tft.drawFastHLine(0, 61, 240, TFT_GREEN);
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(2);
  tft.drawCentreString("PAKIZA", 120, 12, 2);
  tft.setTextColor(0x5E3F, 0x0110);
  tft.setTextSize(1);
  tft.drawCentreString("SOFTWARE Ltd", 120, 38, 1);

  // 2. Center Access Card (Green theme)
  tft.fillRoundRect(10, 72, 220, 236, 12, 0x01A0); // Deep Forest Green card
  tft.drawRoundRect(10, 72, 220, 236, 12, TFT_GREEN);
  tft.drawRoundRect(11, 73, 218, 234, 11, TFT_GREEN);

  // Big Green Status Badge
  tft.fillCircle(120, 108, 22, TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setTextSize(2);
  tft.drawCentreString("OK", 120, 100, 2);

  // Title: "ACCESS GRANTED"
  tft.setTextColor(TFT_GREEN, 0x01A0);
  tft.setTextSize(2);
  tft.drawCentreString("ACCESS GRANTED", 120, 138, 2);

  // Employee Name
  tft.setTextColor(TFT_WHITE, 0x01A0);
  if (name.length() > 0) {
    if (name.length() > 14) {
      tft.setTextSize(1);
      tft.drawCentreString(name, 120, 172, 2);
    } else {
      tft.setTextSize(1);
      tft.drawCentreString(name, 120, 168, 2);
    }
  } else {
    tft.setTextSize(1);
    tft.drawCentreString("AUTHORIZED USER", 120, 168, 2);
  }

  // Employee ID Badge Pill
  if (empId.length() > 0) {
    tft.fillRoundRect(30, 204, 180, 32, 6, 0x00A0);
    tft.drawRoundRect(30, 204, 180, 32, 6, 0x5E3F);
    tft.setTextColor(TFT_CYAN, 0x00A0);
    tft.setTextSize(2);
    String idDisplay = "ID: " + empId;
    tft.drawCentreString(idDisplay, 120, 212, 1);
  }

  // Door Status Indicator
  tft.setTextColor(TFT_YELLOW, 0x01A0);
  tft.setTextSize(1);
  tft.drawCentreString("DOOR UNLOCKED", 120, 262, 2);
}

// ================= ENTER IDLE STATE =================
void enterIdle() {
  closeDoor(); // Lock door (Safe State)
  drawIdleScreen();
  activeMode = false;
  currentEmpId = "";
  currentName = "";
  Serial.println("[System] Idle Standby active.");
}

// ================= PARAMETER EXTRACTION HELPERS =================
String getNameFromRequest() {
  if (server.hasArg("name"))
    return server.arg("name");
  if (server.hasArg("employee_name"))
    return server.arg("employee_name");
  if (server.hasArg("emp_name"))
    return server.arg("emp_name");
  if (server.hasArg("empName"))
    return server.arg("empName");
  if (server.hasArg("userName"))
    return server.arg("userName");
  if (server.hasArg("user_name"))
    return server.arg("user_name");
  if (server.hasArg("full_name"))
    return server.arg("full_name");
  if (server.hasArg("fullName"))
    return server.arg("fullName");

  // Fallback: search across all arguments for key containing 'name'
  for (int i = 0; i < server.args(); i++) {
    String argName = server.argName(i);
    argName.toLowerCase();
    if (argName.indexOf("name") >= 0) {
      return server.arg(i);
    }
  }
  return "";
}

String getIdFromRequest() {
  if (server.hasArg("employee_id"))
    return server.arg("employee_id");
  if (server.hasArg("emp"))
    return server.arg("emp");
  if (server.hasArg("id"))
    return server.arg("id");
  if (server.hasArg("emp_id"))
    return server.arg("emp_id");
  if (server.hasArg("empId"))
    return server.arg("empId");
  if (server.hasArg("userId"))
    return server.arg("userId");
  if (server.hasArg("user_id"))
    return server.arg("user_id");

  // Fallback: search across all arguments for key containing 'id' or 'emp'
  for (int i = 0; i < server.args(); i++) {
    String argName = server.argName(i);
    argName.toLowerCase();
    if (argName.indexOf("id") >= 0 || argName.indexOf("emp") >= 0) {
      if (argName.indexOf("url") < 0 && argName.indexOf("pic") < 0) {
        return server.arg(i);
      }
    }
  }
  return "";
}

// ================= UNIFIED UNLOCK HANDLER (WITH DEBOUNCING / ANTI-GLITCH)
// =================
void processUnlockRequest(const char *source) {
  String empId = getIdFromRequest();
  String name = getNameFromRequest();

  // Decode URL spaces
  name.replace("+", " ");
  name.replace("%20", " ");
  empId.trim();
  name.trim();

  // If new request has empty name, but same empId was already known with a
  // name, retain it!
  if (name.length() == 0 && empId.length() > 0 && empId == currentEmpId &&
      currentName.length() > 0) {
    name = currentName;
  }

  Serial.println("\n==========================================");
  Serial.print("[API Request] Source: ");
  Serial.println(source);
  Serial.print("[Employee Name]: ");
  Serial.println(name.length() > 0 ? name : "(None)");
  Serial.print("[Employee ID]  : ");
  Serial.println(empId.length() > 0 ? empId : "(None)");
  Serial.println("==========================================");

  // ANTI-GLITCH / DEBOUNCE:
  // If door is already active and unlocked, do not redraw the screen to prevent
  // flickering!
  if (activeMode && (millis() - actionStart < 5000)) {
    actionStart = millis(); // Extend the 5-second timer
    openDoor();             // Keep door open
    Serial.println("[Debounce] Door already open. Extended timer without "
                   "redrawing screen.");
    server.send(200, "text/plain", "DOOR UNLOCKED");
    return;
  }

  currentEmpId = empId;
  currentName = name;

  // 1. Draw Clean Full-Screen Access Card
  drawAccessCard(name, empId);

  // 2. Unlock Physical Door
  openDoor();

  // 3. Mark state as active (5 second auto-lock window)
  actionStart = millis();
  activeMode = true;

  // 4. Send 200 OK
  server.send(200, "text/plain", "DOOR UNLOCKED");
}

void handleOn() { processUnlockRequest("/on"); }

void handleSilent() { processUnlockRequest("/silent"); }

void handleRotation() {
  if (server.hasArg("r")) {
    currentRotation = server.arg("r").toInt();
  } else if (server.hasArg("val")) {
    currentRotation = server.arg("val").toInt();
  }
  Serial.print("[Display] Rotation changed dynamically to: ");
  Serial.println(currentRotation);
  if (activeMode) {
    drawAccessCard(currentName, currentEmpId);
  } else {
    drawIdleScreen();
  }
  String resp = "Rotation set to: " + String(currentRotation) + 
                " (Width: " + String(tft.width()) + ", Height: " + String(tft.height()) + ")";
  server.send(200, "text/plain", resp);
}

void handleInvert() {
  bool inv = true;
  if (server.hasArg("mode")) {
    inv = (server.arg("mode").toInt() == 1);
  }
  tft.invertDisplay(inv);
  Serial.print("[Display] Inversion set to: ");
  Serial.println(inv ? "ON" : "OFF");
  server.send(200, "text/plain", inv ? "Inversion ON" : "Inversion OFF");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // Initialize GPIO pins
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Turn ON Backlight (Active-LOW on CYD: LOW = ON)
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  // SAFE START → DOOR CLOSED
  closeDoor();

  // Initialize TFT Display
  tft.init();
  tft.setRotation(currentRotation); // Portrait (240x320)
  tft.fillScreen(TFT_WHITE);

  Serial.println("\n==========================================");
  Serial.println("[Display Diagnostics]");
  Serial.print("  Resolution: ");
  Serial.print(tft.width());
  Serial.print(" x ");
  Serial.println(tft.height());
  Serial.print("  Rotation  : ");
  Serial.println(currentRotation);
  Serial.println("==========================================");

  // Boot Screen
  tft.fillRect(0, 0, 240, 62, 0x0110);
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(2);
  tft.drawCentreString("PAKIZA SOFTWARE LTD", 120, 20, 2);
  tft.setTextColor(0x0110, TFT_WHITE);
  tft.setTextSize(1);
  tft.drawCentreString("Connecting to WiFi...", 120, 160, 2);

  // Connect to WiFi
  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(400);
    Serial.print(".");
    attempts++;
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Setup Server Endpoints
  server.on("/on", handleOn);
  server.on("/silent", handleSilent);
  server.on("/unlock", handleOn);
  server.on("/open", handleOn);
  server.on("/rotation", handleRotation);
  server.on("/invert", handleInvert);
  server.begin();
  Serial.println("[Server] HTTP Server started.");

  // Draw Standby Screen
  enterIdle();
}

// ================= MAIN LOOP =================
void loop() {
  server.handleClient();

  // Auto-lock door and return to Standby screen after 5 seconds

  if (activeMode && millis() - actionStart > 5000) {
    enterIdle();
  }
}
