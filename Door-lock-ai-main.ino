#include "lgfx_CYD.hpp"
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

LGFX tft;                // LovyanGFX object
int currentRotation = 0; // 6: Portrait Un-mirrored (Full Screen 240x320)

// ================= WIFI =================
const char *ssid = "HitchHiker";
const char *password = "noc@pkcl";
IPAddress local_IP(10, 81, 100, 72);
IPAddress gateway(10, 81, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

WebServer server(80);

// ================= PINS =================
#define RELAY_PIN 22
#define GREEN_LED 17

// ================= STATE =================
const unsigned long UNLOCK_DURATION = 10000;
unsigned long actionStart = 0;
bool activeMode = false;
String currentEmpId = "";
String currentName = "";
String currentTimeStr = "";
String currentDateStr = "";

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

// ================= RELAY =================
void openDoor() {
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(GREEN_LED, HIGH);
}
void closeDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, LOW);
}

// ================= TIME & DATE =================
String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    if (currentTimeStr.length() > 0)
      return currentTimeStr;
    return "--:--";
  }
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

String getCurrentDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    if (currentDateStr.length() > 0)
      return currentDateStr;
    return "--/--/----";
  }
  char buf[16];
  strftime(buf, sizeof(buf), "%d-%m-%Y", &timeinfo);
  return String(buf);
}

// ================= IDLE SCREEN =================
void drawIdleScreen() {
  tft.setRotation(currentRotation);
  tft.fillScreen(TFT_WHITE);
  int w = tft.width();
  int h = tft.height();

  if (h >= w) {
    // ======== FULL-SCREEN PORTRAIT (Dynamic 240x320 or 320x480) ========
    // 1. Header Banner
    tft.fillRect(0, 0, w, 56, 0x0110);
    tft.drawFastHLine(0, 55, w, 0x039F);
    tft.setTextColor(TFT_WHITE, 0x0110);
    tft.setTextSize(2);
    tft.drawCenterString("PAKIZA", w / 2, 10);
    tft.setTextColor(0x5E3F, 0x0110);
    tft.setTextSize(1);
    tft.drawCenterString("SOFTWARE LTD", w / 2, 34);

    // 2. Center Face Recognition Standby Card
    int cardY = 64;
    int footerH = 44;
    int cardH = h - cardY - footerH - 16;
    tft.fillRoundRect(8, cardY, w - 16, cardH, 12, 0xF7BE);
    tft.drawRoundRect(8, cardY, w - 16, cardH, 12, 0xC618);

    // Camera circle target
    int cx = w / 2;
    int cy = cardY + (cardH / 3);
    tft.drawCircle(cx, cy, 38, 0x0110);
    tft.drawCircle(cx, cy, 37, 0x0110);
    tft.fillCircle(cx, cy, 6, TFT_GREEN);

    // Corner brackets for facial scanner
    tft.drawFastHLine(cx - 50, cy - 50, 20, 0x0110);
    tft.drawFastVLine(cx - 50, cy - 50, 20, 0x0110);
    tft.drawFastHLine(cx + 30, cy - 50, 20, 0x0110);
    tft.drawFastVLine(cx + 50, cy - 50, 20, 0x0110);
    tft.drawFastHLine(cx - 50, cy + 50, 20, 0x0110);
    tft.drawFastVLine(cx - 50, cy + 30, 20, 0x0110);
    tft.drawFastHLine(cx + 30, cy + 50, 20, 0x0110);
    tft.drawFastVLine(cx + 50, cy + 30, 20, 0x0110);

    tft.setTextColor(0x0110, 0xF7BE);
    tft.setTextSize(2);
    tft.drawCenterString("READY TO SCAN", cx, cy + 55);

    tft.setTextColor(0x7BEF, 0xF7BE);
    tft.setTextSize(1);
    tft.drawCenterString("Please look at camera", cx, cy + 90);

    // 3. Footer Banner: Online Status & IP
    int footerY = h - footerH - 8;
    tft.fillRoundRect(8, footerY, w - 16, footerH, 8, 0x0110);
    tft.fillCircle(24, footerY + (footerH / 2), 4, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, 0x0110);
    tft.setTextSize(1);
    tft.drawString("SYSTEM ONLINE", 36, footerY + (footerH / 2) - 4);
    tft.setTextColor(0x5E3F, 0x0110);
    tft.drawRightString(WiFi.localIP().toString(), w - 16,
                        footerY + (footerH / 2) - 4);
  } else {
    // ======== FULL-SCREEN LANDSCAPE (320 x 240) ========
    // 1. Header
    tft.fillRect(0, 0, 320, 42, 0x0110);
    tft.drawFastHLine(0, 41, 320, 0x039F);
    tft.setTextColor(TFT_WHITE, 0x0110);
    tft.setTextSize(2);
    tft.drawCenterString("PAKIZA SOFTWARE LTD", 160, 12);

    // 2. Card (Edge-to-edge)
    tft.fillRoundRect(8, 46, 304, 152, 10, 0xF7BE);
    tft.drawRoundRect(8, 46, 304, 152, 10, 0xC618);

    int cx = 160, cy = 96;
    tft.drawCircle(cx, cy, 28, 0x0110);
    tft.drawCircle(cx, cy, 27, 0x0110);
    tft.fillCircle(cx, cy, 5, TFT_GREEN);

    tft.setTextColor(0x0110, 0xF7BE);
    tft.setTextSize(2);
    tft.drawCenterString("READY TO SCAN", 160, 138);

    tft.setTextColor(0x7BEF, 0xF7BE);
    tft.setTextSize(1);
    tft.drawCenterString("Please look at camera", 160, 166);

    // 3. Footer (Edge-to-edge)
    tft.fillRoundRect(8, 202, 304, 34, 6, 0x0110);
    tft.fillCircle(22, 219, 4, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, 0x0110);
    tft.setTextSize(1);
    tft.drawString("SYSTEM ONLINE", 34, 214);
    tft.drawRightString(WiFi.localIP().toString(), 302, 214);
  }
}

// ================= ACCESS GRANTED =================
void drawAccessCard(String name, String empId, String accessTime = "",
                    String accessDate = "") {
  tft.setRotation(currentRotation);
  tft.fillScreen(TFT_WHITE);
  int w = tft.width();
  int h = tft.height();

  String displayName =
      name.length() > 0 ? name
                        : (empId.length() > 0 ? "ID: " + empId : "AUTHORIZED");

  String showTime = accessTime.length() > 0 ? accessTime : getCurrentTime();
  String showDate = accessDate.length() > 0 ? accessDate : getCurrentDate();

  if (h >= w) {
    // ======== FULL-SCREEN PORTRAIT (Dynamic 240x320 or 320x480) ========
    // 1. Header
    tft.fillRect(0, 0, w, 56, 0x0110);
    tft.drawFastHLine(0, 55, w, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, 0x0110);
    tft.setTextSize(2);
    tft.drawCenterString("PAKIZA", w / 2, 10);
    tft.setTextColor(0x5E3F, 0x0110);
    tft.setTextSize(1);
    tft.drawCenterString("SOFTWARE LTD", w / 2, 34);

    // 2. Green Access Card (Edge-to-Edge, full height)
    int cardY = 64;
    int cardH = h - cardY - 12;
    tft.fillRoundRect(8, cardY, w - 16, cardH, 12, 0x01A0);
    tft.drawRoundRect(8, cardY, w - 16, cardH, 12, TFT_GREEN);

    // Green OK badge
    tft.fillCircle(w / 2, cardY + 36, 22, TFT_GREEN);
    tft.setTextColor(0x01A0, TFT_GREEN);
    tft.setTextSize(2);
    tft.drawCenterString("OK", w / 2, cardY + 28);

    // Title
    tft.setTextColor(TFT_GREEN, 0x01A0);
    tft.setTextSize(2);
    tft.drawCenterString("ACCESS GRANTED", w / 2, cardY + 68);

    // Name Box
    tft.fillRoundRect(16, cardY + 100, w - 32, 34, 6, 0x00A0);
    tft.setTextColor(TFT_WHITE, 0x00A0);
    tft.setTextSize(2);
    tft.drawCenterString(displayName, w / 2, cardY + 108);

    // ID Box
    if (empId.length() > 0) {
      tft.fillRoundRect(16, cardY + 144, w - 32, 32, 6, 0x00A0);
      tft.setTextColor(TFT_CYAN, 0x00A0);
      tft.setTextSize(2);
      tft.drawCenterString("ID: " + empId, w / 2, cardY + 151);
    }

    // Date & Time
    tft.setTextColor(TFT_YELLOW, 0x01A0);
    tft.setTextSize(2);
    if (showDate.length() > 0 && showDate != "--/--/----") {
      tft.drawCenterString("Date: " + showDate, w / 2, cardY + 188);
      tft.drawCenterString("Time: " + showTime, w / 2, cardY + 218);
      tft.drawCenterString("DOOR UNLOCKED", w / 2, cardY + 258);
      tft.fillRoundRect(24, cardY + 288, w - 48, 6, 3, TFT_GREEN);
    } else {
      tft.drawCenterString("Time: " + showTime, w / 2, cardY + 195);
      tft.drawCenterString("DOOR UNLOCKED", w / 2, cardY + 235);
      tft.fillRoundRect(24, cardY + 265, w - 48, 6, 3, TFT_GREEN);
    }
  } else {
    // ======== FULL-SCREEN LANDSCAPE (320 x 240) ========
    // 1. Header
    tft.fillRect(0, 0, 320, 42, 0x0110);
    tft.drawFastHLine(0, 41, 320, TFT_GREEN);
    tft.setTextColor(TFT_WHITE, 0x0110);
    tft.setTextSize(2);
    tft.drawCenterString("PAKIZA SOFTWARE LTD", 160, 12);

    // 2. Card (Edge-to-edge)
    tft.fillRoundRect(8, 46, 304, 188, 10, 0x01A0);
    tft.drawRoundRect(8, 46, 304, 188, 10, TFT_GREEN);

    // Title
    tft.setTextColor(TFT_GREEN, 0x01A0);
    tft.setTextSize(2);
    tft.drawCenterString("ACCESS GRANTED", 160, 56);

    // Name
    tft.fillRoundRect(16, 84, 288, 28, 6, 0x00A0);
    tft.setTextColor(TFT_WHITE, 0x00A0);
    tft.setTextSize(2);
    tft.drawCenterString(displayName, 160, 90);

    // ID
    if (empId.length() > 0) {
      tft.fillRoundRect(16, 118, 288, 26, 6, 0x00A0);
      tft.setTextColor(TFT_CYAN, 0x00A0);
      tft.setTextSize(2);
      tft.drawCenterString("ID: " + empId, 160, 123);
    }

    // Time & Date Status
    tft.setTextColor(TFT_YELLOW, 0x01A0);
    tft.setTextSize(2);
    if (showDate.length() > 0 && showDate != "--/--/----") {
      tft.drawCenterString(showDate + "  " + showTime, 160, 154);
    } else {
      tft.drawCenterString("Time: " + showTime, 160, 154);
    }

    tft.setTextSize(1);
    tft.drawCenterString("DOOR UNLOCKED", 160, 184);

    tft.fillRoundRect(30, 214, 260, 4, 2, TFT_GREEN);
  }
}

void enterIdle() {
  closeDoor();
  drawIdleScreen();
  activeMode = false;
  currentEmpId = "";
  currentName = "";
  currentTimeStr = "";
  currentDateStr = "";
}

// ================= HELPERS & PARSERS =================
String urlDecode(String str) {
  String decoded = "";
  char temp[] = "0x00";
  int len = str.length();
  int i = 0;
  while (i < len) {
    char c = str[i];
    if (c == '+') {
      decoded += ' ';
      i++;
    } else if (c == '%' && i + 2 < len) {
      temp[2] = str[i + 1];
      temp[3] = str[i + 2];
      char val = (char)strtol(temp, NULL, 16);
      if (val != 0) {
        decoded += val;
      } else {
        decoded += c;
      }
      i += 3;
    } else {
      decoded += c;
      i++;
    }
  }
  return decoded;
}

String cleanString(String s) {
  s = urlDecode(s);
  s.replace("\"", "");
  s.replace("\\", "");
  s.replace("+", " ");
  s.trim();
  return s;
}

String getRawBody() {
  if (server.hasArg("plain"))
    return server.arg("plain");
  if (server.hasArg("payload"))
    return server.arg("payload");
  if (server.hasArg("body"))
    return server.arg("body");
  if (server.hasArg("data"))
    return server.arg("data");
  return "";
}

String extractJsonValue(const String &body, const String &key) {
  if (body.length() == 0)
    return "";

  int keyIdx = body.indexOf("\"" + key + "\"");
  if (keyIdx < 0) {
    keyIdx = body.indexOf(key);
    if (keyIdx < 0)
      return "";
  }

  int colonIdx = body.indexOf(':', keyIdx + key.length());
  if (colonIdx < 0)
    return "";

  int startIdx = colonIdx + 1;
  while (startIdx < (int)body.length() &&
         (body[startIdx] == ' ' || body[startIdx] == '\t' ||
          body[startIdx] == '\r' || body[startIdx] == '\n')) {
    startIdx++;
  }
  if (startIdx >= (int)body.length())
    return "";

  if (body[startIdx] == '\"') {
    startIdx++;
    int endIdx = body.indexOf('\"', startIdx);
    if (endIdx < 0)
      return "";
    return body.substring(startIdx, endIdx);
  } else {
    int endIdx = startIdx;
    while (endIdx < (int)body.length() && body[endIdx] != ',' &&
           body[endIdx] != '}' && body[endIdx] != ']' && body[endIdx] != '\r' &&
           body[endIdx] != '\n') {
      endIdx++;
    }
    return body.substring(startIdx, endIdx);
  }
}

void syncSystemTime(String timeStr, String dateStr) {
  if (timeStr.length() < 4)
    return;

  int hour = 0, min = 0, sec = 0;
  sscanf(timeStr.c_str(), "%d:%d:%d", &hour, &min, &sec);

  int day = 17, month = 9, year = 2026;
  if (dateStr.length() >= 8) {
    if (dateStr.indexOf('-') > 0) {
      sscanf(dateStr.c_str(), "%d-%d-%d", &day, &month, &year);
    } else if (dateStr.indexOf('/') > 0) {
      sscanf(dateStr.c_str(), "%d/%d/%d", &day, &month, &year);
    }
  }

  struct tm tm_info;
  memset(&tm_info, 0, sizeof(tm_info));
  tm_info.tm_hour = hour;
  tm_info.tm_min = min;
  tm_info.tm_sec = sec;
  tm_info.tm_year = (year >= 1970) ? (year - 1900) : (2026 - 1900);
  tm_info.tm_mon = (month >= 1 && month <= 12) ? (month - 1) : 8;
  tm_info.tm_mday = (day >= 1 && day <= 31) ? day : 17;

  time_t t = mktime(&tm_info);
  if (t != (time_t)-1) {
    struct timeval tv = {.tv_sec = t, .tv_usec = 0};
    settimeofday(&tv, NULL);
    Serial.printf(
        "[TIME] System clock synchronized to %02d:%02d:%02d %02d-%02d-%04d\n",
        hour, min, sec, tm_info.tm_mday, tm_info.tm_mon + 1,
        tm_info.tm_year + 1900);
  }
}

String getNameFromRequest() {
  String val = "";

  // 1. HIGHER PRIORITY: Request Payload (JSON body)
  String body = getRawBody();
  if (body.length() > 0) {
    val = extractJsonValue(body, "employee_name");
    if (val.length() == 0)
      val = extractJsonValue(body, "name");
    if (val.length() == 0)
      val = extractJsonValue(body, "emp_name");
    if (val.length() == 0)
      val = extractJsonValue(body, "empName");
    if (val.length() == 0)
      val = extractJsonValue(body, "userName");
    if (val.length() == 0)
      val = extractJsonValue(body, "user_name");
    if (val.length() == 0)
      val = extractJsonValue(body, "full_name");
    if (val.length() == 0)
      val = extractJsonValue(body, "fullName");
  }

  // 2. FALLBACK: URL Query / Form arguments
  if (val.length() == 0) {
    if (server.hasArg("employee_name"))
      val = server.arg("employee_name");
    else if (server.hasArg("name"))
      val = server.arg("name");
    else if (server.hasArg("emp_name"))
      val = server.arg("emp_name");
    else if (server.hasArg("empName"))
      val = server.arg("empName");
    else if (server.hasArg("userName"))
      val = server.arg("userName");
    else if (server.hasArg("user_name"))
      val = server.arg("user_name");
    else if (server.hasArg("full_name"))
      val = server.arg("full_name");
    else if (server.hasArg("fullName"))
      val = server.arg("fullName");
  }

  // 3. Fallback: scan all argument names
  if (val.length() == 0) {
    for (int i = 0; i < server.args(); i++) {
      String a = server.argName(i);
      a.toLowerCase();
      if (a.indexOf("name") >= 0) {
        val = server.arg(i);
        break;
      }
    }
  }

  return cleanString(val);
}

String getIdFromRequest() {
  String val = "";

  // 1. HIGHER PRIORITY: Request Payload (JSON body)
  String body = getRawBody();
  if (body.length() > 0) {
    val = extractJsonValue(body, "employee_id");
    if (val.length() == 0)
      val = extractJsonValue(body, "id");
    if (val.length() == 0)
      val = extractJsonValue(body, "emp_id");
    if (val.length() == 0)
      val = extractJsonValue(body, "empId");
    if (val.length() == 0)
      val = extractJsonValue(body, "emp");
    if (val.length() == 0)
      val = extractJsonValue(body, "userId");
    if (val.length() == 0)
      val = extractJsonValue(body, "user_id");
  }

  // 2. FALLBACK: URL Query / Form arguments
  if (val.length() == 0) {
    if (server.hasArg("employee_id"))
      val = server.arg("employee_id");
    else if (server.hasArg("id"))
      val = server.arg("id");
    else if (server.hasArg("emp_id"))
      val = server.arg("emp_id");
    else if (server.hasArg("empId"))
      val = server.arg("empId");
    else if (server.hasArg("emp"))
      val = server.arg("emp");
    else if (server.hasArg("userId"))
      val = server.arg("userId");
    else if (server.hasArg("user_id"))
      val = server.arg("user_id");
  }

  // 3. Fallback: scan all argument names
  if (val.length() == 0) {
    for (int i = 0; i < server.args(); i++) {
      String a = server.argName(i);
      a.toLowerCase();
      if ((a.indexOf("id") >= 0 || a.indexOf("emp") >= 0) &&
          a.indexOf("url") < 0 && a.indexOf("pic") < 0) {
        val = server.arg(i);
        break;
      }
    }
  }

  return cleanString(val);
}

String getTimeFromRequest() {
  String val = "";

  // 1. HIGHER PRIORITY: Request Payload (JSON body)
  String body = getRawBody();
  if (body.length() > 0) {
    val = extractJsonValue(body, "time");
    if (val.length() == 0)
      val = extractJsonValue(body, "timestamp");
    if (val.length() == 0)
      val = extractJsonValue(body, "time_str");
    if (val.length() == 0)
      val = extractJsonValue(body, "datetime");
  }

  // 2. FALLBACK: URL Query / Form arguments
  if (val.length() == 0) {
    if (server.hasArg("time"))
      val = server.arg("time");
    else if (server.hasArg("time_str"))
      val = server.arg("time_str");
    else if (server.hasArg("timestamp"))
      val = server.arg("timestamp");
    else if (server.hasArg("datetime"))
      val = server.arg("datetime");
  }

  return cleanString(val);
}

String getDateFromRequest() {
  String val = "";

  // 1. HIGHER PRIORITY: Request Payload (JSON body)
  String body = getRawBody();
  if (body.length() > 0) {
    val = extractJsonValue(body, "date");
    if (val.length() == 0)
      val = extractJsonValue(body, "date_str");
    if (val.length() == 0)
      val = extractJsonValue(body, "date_time");
  }

  // 2. FALLBACK: URL Query / Form arguments
  if (val.length() == 0) {
    if (server.hasArg("date"))
      val = server.arg("date");
    else if (server.hasArg("date_str"))
      val = server.arg("date_str");
  }

  return cleanString(val);
}

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
    for (int i = 1; i < MAX_RECENT; i++)
      if (recentUsers[i].unlockTime < recentUsers[oldest].unlockTime)
        oldest = i;
    recentUsers[oldest] = {empId, name, uTime};
  }
}

void sendResponse(int code, bool success, const String &msg,
                  const String &empId = "", const String &name = "",
                  const String &timeStr = "", const String &dateStr = "") {
  String json = "{";
  json += "\"status\":\"" + String(success ? "success" : "failure") + "\",";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + msg + "\"";
  if (empId.length() > 0)
    json += ",\"employee_id\":\"" + empId + "\"";
  if (name.length() > 0)
    json += ",\"employee_name\":\"" + name + "\"";
  if (timeStr.length() > 0)
    json += ",\"time\":\"" + timeStr + "\"";
  if (dateStr.length() > 0)
    json += ",\"date\":\"" + dateStr + "\"";
  json += "}";
  server.send(code, "application/json", json);
}

void processUnlockRequest(const char *source) {
  String empId = getIdFromRequest();
  String name = getNameFromRequest();
  String reqTime = getTimeFromRequest();
  String reqDate = getDateFromRequest();

  if (reqTime.length() > 0 || reqDate.length() > 0) {
    syncSystemTime(reqTime, reqDate);
    currentTimeStr = reqTime;
    currentDateStr = reqDate;
  } else {
    currentTimeStr = getCurrentTime();
    currentDateStr = getCurrentDate();
  }

  Serial.printf(
      "[REQ] Source: %s, ID: '%s', Name: '%s', Time: '%s', Date: '%s'\n",
      source, empId.c_str(), name.c_str(), reqTime.c_str(), reqDate.c_str());

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
  if (userIdx >= 0) {
    if (millis() - recentUsers[userIdx].unlockTime < SAME_PERSON_COOLDOWN)
      isOnCooldown = true;
  }

  if (empId.length() == 0 && name.length() == 0) {
    if (activeMode || (millis() - lastAnonymousUnlock < SAME_PERSON_COOLDOWN)) {
      sendResponse(200, false, "COOLDOWN ACTIVE");
      return;
    }
    lastAnonymousUnlock = millis();
  }

  if (activeMode && isOnCooldown) {
    if (name.length() > 0 && currentName.length() == 0) {
      currentName = name;
      drawAccessCard(name, currentEmpId, currentTimeStr, currentDateStr);
    }
    sendResponse(200, true, "DOOR UNLOCKED", currentEmpId, currentName,
                 currentTimeStr, currentDateStr);
    return;
  }

  if (!activeMode && isOnCooldown) {
    sendResponse(200, false, "COOLDOWN ACTIVE", empId, name);
    return;
  }

  currentEmpId = empId;
  currentName = name;
  addOrUpdateRecentUser(empId, name, millis());

  drawAccessCard(name, empId, currentTimeStr, currentDateStr);
  openDoor();
  actionStart = millis();
  activeMode = true;

  sendResponse(200, true, "DOOR UNLOCKED", empId, name, currentTimeStr,
               currentDateStr);
}

void handleOn() { processUnlockRequest("/on"); }
void handleSilent() { processUnlockRequest("/silent"); }
void handleRotation() {
  if (server.hasArg("r")) {
    currentRotation = server.arg("r").toInt();
  }
  tft.setRotation(currentRotation);
  if (activeMode) {
    drawAccessCard(currentName, currentEmpId, currentTimeStr, currentDateStr);
  } else {
    drawIdleScreen();
  }
  String resp = "OK: Rotation set to " + String(currentRotation) +
                " (Width: " + String(tft.width()) +
                ", Height: " + String(tft.height()) + ")";
  server.send(200, "text/plain", resp);
}

void handleInvert() {
  bool inv = true;
  if (server.hasArg("mode")) {
    inv = (server.arg("mode").toInt() == 1);
  }
  tft.invertDisplay(inv);
  if (activeMode) {
    drawAccessCard(currentName, currentEmpId, currentTimeStr, currentDateStr);
  } else {
    drawIdleScreen();
  }
  server.send(200, "text/plain", inv ? "Invert ON" : "Invert OFF");
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Turn ON Backlight (Active-LOW on CYD)
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  closeDoor();

  tft.init();
  tft.setRotation(currentRotation);
  tft.fillScreen(TFT_BLACK);

  configTime(6 * 3600, 0, "pool.ntp.org");

  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());

  server.on("/on", handleOn);
  server.on("/silent", handleSilent);
  server.on("/unlock", handleOn);
  server.on("/open", handleOn);
  server.on("/rotation", handleRotation);
  server.on("/invert", handleInvert);
  server.begin();

  enterIdle();
}

void loop() {
  server.handleClient();
  if (activeMode && millis() - actionStart >= UNLOCK_DURATION) {
    enterIdle();
  }
}