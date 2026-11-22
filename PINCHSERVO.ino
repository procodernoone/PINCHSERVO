/*
  PINCHSERVO v1.1 — ESP32 firmware
  Compatible with the supplied HTML/JavaScript.

  Browser -> ESP32:
    GET http://ESP32_IP/                  connection test
    GET http://ESP32_IP/set?angle=120     servo command

  ESP32 -> Browser:
    /       : PINCHSERVO READY
    /set... : {"status":"ok","angle":120}

  Wiring (change pins/address below if needed):
    SG90 signal: GPIO 18
    I2C LCD SDA: GPIO 21
    I2C LCD SCL: GPIO 22
    LCD address: 0x27 (some modules use 0x3F)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ---------- YOUR WIFI DETAILS ----------
const char* WIFI_SSID     = "ASTRO_ROVER";
const char* WIFI_PASSWORD = "baked_potato1897";
// ---------------------------------------

constexpr int SERVO_PIN = 18;
constexpr int LCD_SDA_PIN = 21;
constexpr int LCD_SCL_PIN = 22;
constexpr uint8_t LCD_ADDRESS = 0x27;

constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2400;

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
WebServer server(80);
Servo pinchServo;

int currentAngle = 90;

void addCorsHeaders() {
  // Allows the HTML page (including file:// pages) to make requests.
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-store");
}

void handleRoot() {
  addCorsHeaders();
  server.send(200, "text/plain", "PINCHSERVO READY");
}

void handleSetAngle() {
  if (!server.hasArg("angle")) {
    addCorsHeaders();
    server.send(400, "application/json",
                "{\"status\":\"error\",\"message\":\"Missing angle\"}");
    return;
  }

  // Validate the complete parameter rather than silently accepting text.
  String arg = server.arg("angle");
  char* endPtr = nullptr;
  long requested = strtol(arg.c_str(), &endPtr, 10);

  if (arg.length() == 0 || endPtr == arg.c_str() ||
      *endPtr != '\0' || requested < 0 || requested > 180) {
    addCorsHeaders();
    server.send(400, "application/json",
                "{\"status\":\"error\",\"message\":\"Angle must be 0-180\"}");
    return;
  }

  currentAngle = (int)requested;
  pinchServo.write(currentAngle);

  String json = "{\"status\":\"ok\",\"angle\":" +
                String(currentAngle) + "}";

  addCorsHeaders();
  server.send(200, "application/json", json);

  Serial.printf("Servo command received: %d degrees\n", currentAngle);
}

void handleOptions() {
  addCorsHeaders();
  server.send(204, "text/plain", "");
}

void showConnectedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PINCHSERVO READY");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  Serial.print("Connecting to WiFi");

  const unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
    showConnectedScreen();
  } else {
    Serial.println("WiFi connection failed.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED");
    lcd.setCursor(0, 1);
    lcd.print("Check SSID/pass");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // LCD initialization
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PINCHSERVO");
  lcd.setCursor(0, 1);
  lcd.print("Booting...");

  // Servo initialization
  pinchServo.setPeriodHertz(50);
  pinchServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  pinchServo.write(currentAngle);

  connectToWiFi();

  // These routes exactly match the fetch() calls in the HTML.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_GET, handleSetAngle);

  // Also answer OPTIONS requests for browser compatibility.
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/set", HTTP_OPTIONS, handleOptions);

  server.onNotFound([]() {
    addCorsHeaders();
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  server.handleClient();

  // Retry WiFi without blocking the HTTP server for long periods.
  static unsigned long lastReconnectAttempt = 0;

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastReconnectAttempt >= 10000) {
    lastReconnectAttempt = millis();
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
  }
}
