#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WebServer.h>
#include <WiFi.h>

// ================= WIFI =================
const char *ssid = "HitchHiker";
const char *password = "noc@pkcl";

// Static IP
IPAddress local_IP(10, 81, 100, 72);
IPAddress gateway(10, 81, 100, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

// ================= SERVER =================
WebServer server(80);

// ================= IMAGE URL =================
const char *IMG_PREFIX =
    "http://-----------------/api/v1/health/resize?imageUrl=";
const char *IMG_SUFFIX = "&w=200&h=200";

// ================= SPIFFS PATHS =================
#define BG_ON "/bg4.jpg"
#define BG_IDLE "/bg4.jpg"
#define EMP_PATH "/emp.jpg"

// ================= PINS =================
#define RELAY_PIN 22
#define GREEN_LED 17

// ================= DISPLAY =================
TFT_eSPI tft;

// ================= STATE =================
unsigned long actionStart = 0;
bool activeMode = false;

// ================= JPEG CALLBACK =================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {

  if (y >= tft.height())
    return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// ================= DOWNLOAD EMPLOYEE IMAGE =================
bool downloadEmployee(String url) {

  HTTPClient http;
  WiFiClient client;

  if (!http.begin(client, url))
    return false;

  http.addHeader("Accept", "image/jpeg");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  File file = SPIFFS.open(EMP_PATH, "w");
  if (!file) {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  int len;

  while (http.connected()) {
    len = stream->read(buf, sizeof(buf));
    if (len > 0)
      file.write(buf, len);
    else
      break;
  }

  file.close();
  http.end();
  return true;
}

// ================= SAFE JPG RENDERER =================
bool drawJpgSafe(int16_t x, int16_t y, const char *path) {
  String candidates[4];
  String p = String(path);
  candidates[0] = p.startsWith("/images/") ? p : ("/images" + (p.startsWith("/") ? p : ("/" + p)));
  candidates[1] = p;
  candidates[2] = p.startsWith("/") ? p.substring(1) : ("/" + p);
  candidates[3] = p.startsWith("/images/") ? p.substring(1) : ("images" + (p.startsWith("/") ? p : ("/" + p)));

  String validPath = "";
  for (int i = 0; i < 4; i++) {
    File testFile = SPIFFS.open(candidates[i].c_str(), "r");
    if (testFile) {
      size_t sz = testFile.size();
      testFile.close();
      if (sz > 0) {
        validPath = candidates[i];
        break;
      }
    }
  }

  if (validPath.length() == 0) {
    Serial.print("Error: Image not found or empty in SPIFFS: ");
    Serial.println(path);
    return false;
  }

  Serial.print("Drawing image: ");
  Serial.println(validPath);
  return TJpgDec.drawFsJpg(x, y, validPath.c_str(), SPIFFS);
}

// ================= BACKGROUND =================
void drawBackground(const char *path) {
  tft.setRotation(0); // Portrait Mode
  drawJpgSafe(0, 0, path);
}

// ================= EMPLOYEE IMAGE =================
void drawEmployeeCenter() {
  tft.setRotation(0); // Portrait Mode
  int x = (tft.width() - 200) / 2;  // Centered horizontally: (240 - 200) / 2 = 20
  int y = (tft.height() - 200) / 2; // Centered vertically: (320 - 200) / 2 = 60
  drawJpgSafe(x, y, EMP_PATH);
}

// ================= RELAY LOGIC (INVERTED) =================
// HIGH = CLOSE DOOR
// LOW  = OPEN DOOR

void openDoor() {
  digitalWrite(RELAY_PIN, LOW); // OPEN
  digitalWrite(GREEN_LED, HIGH);
}

void closeDoor() {
  digitalWrite(RELAY_PIN, HIGH); // CLOSE
  digitalWrite(GREEN_LED, LOW);
}

// ================= IDLE MODE =================
void enterIdle() {

  drawBackground(BG_IDLE);

  closeDoor(); // SAFE STATE = CLOSE

  activeMode = false;
}

// ================= /ON HANDLER =================
void handleOn() {

  if (!server.hasArg("empPicUrl")) {
    server.send(400, "text/plain", "Missing empPicUrl");
    return;
  }

  String empPic = server.arg("empPicUrl");
  String fullURL = String(IMG_PREFIX) + empPic + IMG_SUFFIX;

  drawBackground(BG_ON);

  if (downloadEmployee(fullURL)) {
    drawEmployeeCenter();
  }

  openDoor(); // 🚪 OPEN (LOW)

  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "DOOR OPENED");
}

// ================= /SILENT HANDLER =================
void handleSilent() {

  drawBackground(BG_IDLE);

  openDoor(); // 🔓 Open (Low)

  actionStart = millis();
  activeMode = true;

  server.send(200, "text/plain", "DOOR CLOSED");
}

// ================= SPIFFS DEBUG =================
void listSPIFFS() {

  Serial.println("\n=== SPIFFS FILE LIST ===");

  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print(" (");
    Serial.print(file.size());
    Serial.println(" bytes)");
    file = root.openNextFile();
  }

  Serial.println("=== END ===\n");
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Turn on Backlight (Active-LOW on most CYD boards: LOW = ON)
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);

  // Also enable other common backlight pins
  pinMode(27, OUTPUT); digitalWrite(27, HIGH);
  pinMode(4, OUTPUT);  digitalWrite(4, HIGH);
  pinMode(16, OUTPUT); digitalWrite(16, HIGH);

  // SAFE START → DOOR CLOSED
  closeDoor();

  tft.init();
  tft.setRotation(0); // Portrait Mode
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS ERROR");
    return;
  }

  listSPIFFS();

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
  server.begin();

  enterIdle();
}

// ================= LOOP =================
void loop() {

  server.handleClient();

  if (activeMode && millis() - actionStart > 5000) {
    enterIdle();
  }
}