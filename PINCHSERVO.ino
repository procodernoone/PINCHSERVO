#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const int SERVO_PIN = 18;
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;
const int SERVO_START_ANGLE = 90;

const int LCD_SDA_PIN = 21;
const int LCD_SCL_PIN = 22;
const uint8_t LCD_ADDRESS = 0x27;
const uint8_t LCD_COLUMNS = 16;
const uint8_t LCD_ROWS = 2;

const uint16_t HTTP_PORT = 80;
const unsigned long WIFI_RETRY_MS = 15000;

WebServer server(HTTP_PORT);
Servo servo;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

int currentAngle = SERVO_START_ANGLE;
bool serverStarted = false;
wl_status_t lastStatus = WL_NO_SHIELD;
unsigned long lastRetry = 0;

void lcdLine(uint8_t row, String text) {
  if (text.length() > LCD_COLUMNS) text = text.substring(0, LCD_COLUMNS);
  while (text.length() < LCD_COLUMNS) text += ' ';
  lcd.setCursor(0, row);
  lcd.print(text);
}

void sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
  server.sendHeader("Access-Control-Max-Age", "600");
  server.sendHeader("Cache-Control", "no-store");
}

bool parseAngle(const String& s, int& out) {
  if (s.length() == 0 || s.length() > 3) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i])) return false;
  }
  out = s.toInt();
  return out >= 0 && out <= 180;
}

void handleRoot() {
  sendCors();
  server.send(200, "text/plain", "PINCHSERVO READY");
}

void handleSet() {
  sendCors();
  if (!server.hasArg("angle")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"missing angle\"}");
    return;
  }
  int angle;
  if (!parseAngle(server.arg("angle"), angle)) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"angle must be an integer 0-180\"}");
    return;
  }
  servo.write(angle);
  currentAngle = angle;
  server.send(200, "application/json", "{\"status\":\"ok\",\"angle\":" + String(angle) + "}");
}

void handleOptions() {
  sendCors();
  server.send(204);
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptions();
    return;
  }
  sendCors();
  server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"not found\"}");
}

void showWifiState(bool connected) {
  if (connected) {
    lcdLine(0, "PINCHSERVO READY");
    lcdLine(1, WiFi.localIP().toString());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    lcdLine(0, "PINCHSERVO");
    lcdLine(1, "WiFi connecting");
  }
}

void setup() {
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo.write(SERVO_START_ANGLE);

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  showWifiState(false);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/set", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastRetry = millis();
}

void loop() {
  wl_status_t status = WiFi.status();

  if (status != lastStatus) {
    lastStatus = status;
    if (status == WL_CONNECTED) {
      if (!serverStarted) {
        server.begin();
        serverStarted = true;
      }
      showWifiState(true);
    } else {
      showWifiState(false);
    }
  }

  if (status != WL_CONNECTED && millis() - lastRetry > WIFI_RETRY_MS) {
    lastRetry = millis();
    WiFi.reconnect();
  }

  if (serverStarted) server.handleClient();
}
