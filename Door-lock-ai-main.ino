#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

const char *ssid = "HitchHiker";
const char *password = "noc@pkcl";

IPAddress local_IP(10, 81, 100, 72);
IPAddress gateway(10, 81, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

WebServer server(80);

#define RELAY_PIN 22
#define GREEN_LED 17
#define BACKLIGHT_PIN 21

TFT_eSPI tft = TFT_eSPI(320, 240); // Landscape force

const unsigned long UNLOCK_DURATION = 4000;
unsigned long actionStart = 0;
bool activeMode = false;
String currentEmpId = "";
String currentName = "";

struct UnlockedUser {
  String empId;
  String name;
  unsigned long unlockTime;
};

const int MAX_RECENT = 10;
UnlockedUser recentUsers[MAX_RECENT];
int recentUserCount = 0;
unsigned long SAME_PERSON_COOLDOWN = 20000;
unsigned long lastAnonymousUnlock = 0;

int findRecentUser(String empId, String name) {
  for (int i = 0; i < recentUserCount; i++) {
    if (empId.length() > 0 && recentUsers[i].empId == empId)
      return i;
    if (name.length() > 0 && recentUsers[i].name == name)
      return i;
  }
  return -1;
}

void addOrUpdateRecentUser(String empId, String name, unsigned long uTime) {
  int idx = findRecentUser(empId, name);
  if (idx >= 0) {
    recentUsers[idx].unlockTime = uTime;
    if (name.length() > 0)
      recentUsers[idx].name = name;
    if (empId.length() > 0)
      recentUsers[idx].empId = empId;
    return;
  }
  if (recentUserCount < MAX_RECENT) {
    recentUsers[recentUserCount++] = {empId, name, uTime};
  } else {
    int oldest = 0;
    for (int i = 1; i < MAX_RECENT; i++) {
      if (recentUsers[i].unlockTime < recentUsers[oldest].unlockTime)
        oldest = i;
    }
    recentUsers[oldest] = {empId, name, uTime};
  }
}

void openDoor() {
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(GREEN_LED, HIGH);
}

void closeDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, LOW);
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10))
    return "--:--";
  char buf[8];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  return String(buf);
}

// ================= IDLE SCREEN =================
void drawIdleScreen() {
  tft.fillScreen(TFT_WHITE);

  // Header
  tft.fillRect(0, 0, 320, 40, 0x0110);
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(2);
  tft.drawCentreString("PAKIZA SOFTWARE LTD", 160, 8, 2);

  // Card
  tft.fillRoundRect(20, 50, 280, 145, 10, 0xF7BE);
  tft.drawRoundRect(20, 50, 280, 145, 10, 0xC618);

  // Circle
  tft.drawCircle(160, 100, 32, 0x0110);
  tft.drawCircle(160, 100, 31, 0x0110);
  tft.fillCircle(160, 100, 6, TFT_GREEN);

  tft.setTextColor(0x0110, 0xF7BE);
  tft.setTextSize(2);
  tft.drawCentreString("READY TO SCAN", 160, 145, 2);
  tft.setTextColor(0x7BEF, 0xF7BE);
  tft.setTextSize(1);
  tft.drawCentreString("Please look at camera", 160, 170, 1);

  // Footer
  tft.fillRoundRect(20, 205, 280, 28, 6, 0x0110);
  tft.fillCircle(35, 219, 4, TFT_GREEN);
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(1);
  tft.drawString("SYSTEM ONLINE", 48, 214, 1);
  tft.drawRightString(WiFi.localIP().toString(), 290, 214, 1);
}

// ================= ACCESS GRANTED =================
void drawAccessCard(String name, String empId) {
  tft.fillScreen(TFT_WHITE);

  // Header
  tft.fillRect(0, 0, 320, 40, 0x0110);
  tft.setTextColor(TFT_WHITE, 0x0110);
  tft.setTextSize(2);
  tft.drawCentreString("PAKIZA SOFTWARE LTD", 160, 8, 2);

  // Green Card
  tft.fillRoundRect(20, 48, 280, 160, 12, 0x01A0);
  tft.drawRoundRect(20, 48, 280, 160, 12, TFT_GREEN);

  // Title
  tft.setTextColor(TFT_GREEN, 0x01A0);
  tft.setTextSize(2);
  tft.drawCentreString("ACCESS GRANTED", 160, 60, 2);

  // Name
  String displayName =
      name.length() > 0 ? name
                        : (empId.length() > 0 ? "ID: " + empId : "AUTHORIZED");
  tft.fillRoundRect(35, 95, 250, 28, 6, 0x00A0);
  tft.setTextColor(TFT_WHITE, 0x00A0);
  tft.setTextSize(2);
  tft.drawCentreString(displayName, 160, 101, 1);

  // ID
  if (name.length() > 0 && empId.length() > 0) {
    tft.fillRoundRect(35, 130, 250, 26, 6, 0x00A0);
    tft.setTextColor(TFT_CYAN, 0x00A0);
    tft.setTextSize(2);
    tft.drawCentreString("ID: " + empId, 160, 135, 1);
  }

  // Time (Yellow on dark green)
  tft.setTextColor(TFT_YELLOW, 0x01A0);
  tft.setTextSize(2);
  tft.drawCentreString("Time: " + getCurrentTime(), 160, 165, 1);

  // Door status
  tft.setTextColor(TFT_YELLOW, 0x01A0);
  tft.setTextSize(1);
  tft.drawCentreString("DOOR UNLOCKED", 160, 190, 2);
}

void enterIdle() {
  closeDoor();
  drawIdleScreen();
  activeMode = false;
  currentEmpId = "";
  currentName = "";
}

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
  for (int i = 0; i < server.args(); i++) {
    String a = server.argName(i);
    a.toLowerCase();
    if (a.indexOf("name") >= 0)
      return server.arg(i);
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
  for (int i = 0; i < server.args(); i++) {
    String a = server.argName(i);
    a.toLowerCase();
    if ((a.indexOf("id") >= 0 || a.indexOf("emp") >= 0) &&
        a.indexOf("url") < 0 && a.indexOf("pic") < 0)
      return server.arg(i);
  }
  return "";
}

void processUnlockRequest(const char *source) {
  String empId = getIdFromRequest();
  String name = getNameFromRequest();
  name.replace("+", " ");
  name.replace("%20", " ");
  empId.trim();
  name.trim();

  if (name.length() == 0 && empId.length() > 0) {
    if (empId == currentEmpId && currentName.length() > 0)
      name = currentName;
    else {
      int r = findRecentUser(empId, "");
      if (r >= 0)
        name = recentUsers[r].name;
    }
  }

  int userIdx = findRecentUser(empId, name);
  bool isOnCooldown = false;
  unsigned long timeSince = 0;
  if (userIdx >= 0) {
    timeSince = millis() - recentUsers[userIdx].unlockTime;
    if (timeSince < SAME_PERSON_COOLDOWN)
      isOnCooldown = true;
  }

  if (empId.length() == 0 && name.length() == 0) {
    if (activeMode || (millis() - lastAnonymousUnlock < SAME_PERSON_COOLDOWN)) {
      server.send(200, "text/plain", "COOLDOWN ACTIVE");
      return;
    }
    lastAnonymousUnlock = millis();
  }

  if (activeMode && isOnCooldown) {
    if (name.length() > 0 && currentName.length() == 0) {
      currentName = name;
      drawAccessCard(name, currentEmpId);
    }
    server.send(200, "text/plain", "DOOR UNLOCKED");
    return;
  }

  if (!activeMode && isOnCooldown) {
    server.send(200, "text/plain", "COOLDOWN ACTIVE");
    return;
  }

  currentEmpId = empId;
  currentName = name;
  addOrUpdateRecentUser(empId, name, millis());

  drawAccessCard(name, empId);
  openDoor();
  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "DOOR UNLOCKED");
}

void handleOn() { processUnlockRequest("/on"); }
void handleSilent() { processUnlockRequest("/silent"); }

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, LOW);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  closeDoor();

  tft.init();
  tft.setRotation(1);
  // tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);
  delay(50);

  configTime(6 * 3600, 0, "pool.ntp.org");

  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK");
  Serial.println(WiFi.localIP());

  server.on("/on", handleOn);
  server.on("/silent", handleSilent);
  server.on("/unlock", handleOn);
  server.on("/open", handleOn);
  server.begin();

  enterIdle();
}

void loop() {
  server.handleClient();
  if (activeMode && millis() - actionStart >= UNLOCK_DURATION) {
    enterIdle();
  }
}