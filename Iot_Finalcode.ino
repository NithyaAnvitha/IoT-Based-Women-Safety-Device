#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// WiFi credentials
const char* ssid = "Redmi Note 10 Pro";
const char* password = "cherry03";

// Telegram Bot configuration
#define BOT_TOKEN "Define your bot token here"
#define CHAT_ID "Define your chat id here"

// Firebase configuration
#define FIREBASE_PROJECT_ID "women-safety-database-e6824"
#define FIREBASE_URL "https://women-safety-database-e6824-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_API_KEY "1:536911341016:web:0b2f02e4bfe733f0437ead"

// Pin definitions
#define TOUCH_SENSOR_PIN 18 
#define RED_LED 21
#define YELLOW_LED 22
#define GREEN_LED 23
#define GPS_RX_PIN 16  // NEO-6M RX pin (connected to ESP32 TX2)
#define GPS_TX_PIN 17  // NEO-6M TX pin (connected to ESP32 RX2)

// GPS variables
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
float latitude = 0.0;
float longitude = 0.0;
bool gpsLock = false;

// Optimized touch detection variables
volatile bool touchDetected = false;
unsigned long lastTouchTime = 0;
unsigned long touchStartTime = 0;
int touchCount = 0;
const unsigned long TOUCH_WINDOW = 60000;  // 60 seconds
const unsigned long MIN_TOUCH_INTERVAL = 50;

// Device Info
String deviceId;
String deviceName = "WOMEN_SAFETY_DEVICE_01";

// WiFi optimization
const unsigned long WIFI_TIMEOUT = 10000;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
unsigned long lastWiFiCheck = 0;
bool previousWiFiStatus = false;

// Time variables
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST offset
const int daylightOffset_sec = 0;
bool timeInitialized = false;

// Statistics tracking
unsigned long totalTouches = 0;
unsigned long emergencyAlerts = 0;

// Non-blocking LED blink variables
unsigned long ledBlinkStart = 0;
int blinkingLED = -1;
bool ledState = false;
const unsigned long BLINK_DURATION = 500;

// Heartbeat optimization
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 300000;  // 5 minutes

// HTTP timeout optimization
const unsigned long HTTP_TIMEOUT = 5000;

// Pre-allocated JSON documents
DynamicJsonDocument telegramDoc(1024);
DynamicJsonDocument firebaseDoc(512);

// Interrupt service routine for touch detection
void IRAM_ATTR touchISR() {
  touchDetected = true;
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);  // Initialize NEO-6M GPS at 9600 baud

  // Initialize pins
  pinMode(TOUCH_SENSOR_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Turn off all LEDs initially
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  // Generate device ID from MAC address
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");

  // Attach interrupt for touch detection
  attachInterrupt(digitalPinToInterrupt(TOUCH_SENSOR_PIN), touchISR, RISING);

  Serial.println("\n=== ESP32 WOMEN SAFETY DEVICE WITH NEO-6M GPS ===");

  // WiFi setup
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  
  if (connectToWiFi()) {
    digitalWrite(YELLOW_LED, HIGH);
    previousWiFiStatus = true;
    initializeTime();
    sendStartupMessageAsync();
    sendDeviceOnlineStatusAsync();
  } else {
    Serial.println("❌ Failed to connect to WiFi!");
    digitalWrite(YELLOW_LED, LOW);
    previousWiFiStatus = false;
  }

  Serial.println("Touch the sensor 3 times within 60 seconds to send alert");
  Serial.println("===============================================");
}

void loop() {
  unsigned long currentTime = millis();

  // Process GPS data
  updateGPS();

  // Handle touch events
  if (touchDetected) {
    handleTouchEvent();
    touchDetected = false;
  }

  // Non-blocking LED blink handling
  handleLEDBlink(currentTime);

  // Check WiFi status periodically
  if (currentTime - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    handleWiFiStatus();
    lastWiFiCheck = currentTime;
  }

  // Reset touch count if window expires
  if (touchCount > 0 && (currentTime - touchStartTime) > TOUCH_WINDOW) {
    Serial.println("⏰ Touch window expired. Resetting count.");
    touchCount = 0;
  }

  // Send periodic heartbeat
  if (currentTime - lastHeartbeat > HEARTBEAT_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      sendHeartbeatAsync();
    }
    lastHeartbeat = currentTime;
  }

  delay(10);
}

void updateGPS() {
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid() && gps.satellites.value() >= 4) {  // Ensure valid fix
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        gpsLock = true;
        digitalWrite(GREEN_LED, HIGH);
        Serial.printf("📍 GPS Lock: Lat=%.6f, Lon=%.6f, Satellites=%d\n", 
                      latitude, longitude, gps.satellites.value());
      } else {
        gpsLock = false;
        digitalWrite(GREEN_LED, LOW);
        Serial.println("🔍 Waiting for GPS lock...");
      }
    }
  }
}

bool connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime < WIFI_TIMEOUT)) {
    delay(100);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.printf("📡 IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("📶 RSSI: %d dBm\n", WiFi.RSSI());
    return true;
  } else {
    Serial.println("\n❌ WiFi connection timeout!");
    return false;
  }
}

void handleWiFiStatus() {
  bool currentWiFiStatus = (WiFi.status() == WL_CONNECTED);
  
  if (currentWiFiStatus != previousWiFiStatus) {
    if (currentWiFiStatus) {
      Serial.println("✅ WiFi connected! Yellow LED ON");
      digitalWrite(YELLOW_LED, HIGH);
      sendTelegramMessageAsync("🔄 ESP32 reconnected to WiFi");
      sendDeviceOnlineStatusAsync();
    } else {
      Serial.println("⚠ WiFi disconnected! Yellow LED OFF");
      digitalWrite(YELLOW_LED, LOW);
      sendDeviceOfflineStatusAsync();
      Serial.println("🔄 Attempting WiFi reconnection...");
      WiFi.reconnect();
    }
    previousWiFiStatus = currentWiFiStatus;
  }
}

void handleTouchEvent() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastTouchTime < MIN_TOUCH_INTERVAL) {
    return;
  }
  
  lastTouchTime = currentTime;
  totalTouches++;
  
  if (touchCount == 0) {
    touchStartTime = currentTime;
  }
  
  touchCount++;
  Serial.printf("👆 Touch detected! Count: %d/3 (%.1fs elapsed)\n", 
                touchCount, (currentTime - touchStartTime) / 1000.0);

  logTouchEventAsync(touchCount);

  if (touchCount >= 3) {
    unsigned long timeDiff = currentTime - touchStartTime;
    
    if (timeDiff <= TOUCH_WINDOW) {
      Serial.println("🚨 TRIPLE TOUCH DETECTED! Sending emergency alert...");
      startLEDBlink(RED_LED);
      sendEmergencyAlertAsync();
      emergencyAlerts++;
      touchCount = 0;
    } else {
      Serial.println("⏰ Touch window expired. Starting new sequence.");
      touchCount = 1;
      touchStartTime = currentTime;
    }
  } else {
    Serial.printf("⏳ Need %d more touches within %.1f seconds\n", 
                  3 - touchCount, (TOUCH_WINDOW - (currentTime - touchStartTime)) / 1000.0);
  }
}

void initializeTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  timeInitialized = true;
}

void startLEDBlink(int pin) {
  blinkingLED = pin;
  ledBlinkStart = millis();
  ledState = true;
  digitalWrite(pin, HIGH);
}

void handleLEDBlink(unsigned long currentTime) {
  if (blinkingLED == -1) return;
  
  if (currentTime - ledBlinkStart > BLINK_DURATION) {
    digitalWrite(blinkingLED, LOW);
    
    if (blinkingLED == YELLOW_LED && WiFi.status() == WL_CONNECTED) {
      digitalWrite(YELLOW_LED, HIGH);
    } else if (blinkingLED == GREEN_LED && gpsLock) {
      digitalWrite(GREEN_LED, HIGH);
    }
    
    blinkingLED = -1;
  }
}

void sendStartupMessageAsync() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String message = "🟢 ESP32 Women Safety Device Online\n\n";
  message += "✅ System initialized successfully\n";
  message += "📡 Connected to: " + String(ssid) + "\n";
  message += "👆 Touch sensor: Active (Optimized)\n";
  message += "📍 GPS: Waiting for lock\n";
  message += "🆔 Device ID: " + deviceId + "\n";
  message += "📱 Device Name: " + deviceName + "\n\n";
  message += "⚡ Ready for emergency alerts!\n";
  message += "Touch sensor 3 times within 60 seconds to trigger alert";

  sendTelegramMessageAsync(message);
}

void sendEmergencyAlertAsync() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No WiFi for emergency alert!");
    return;
  }

  if (!gpsLock) {
    Serial.println("❌ No GPS lock for emergency alert!");
    return;
  }

  String timestamp = getCurrentTimestamp();
  String message = "🚨 EMERGENCY ALERT! 🚨\n\n";
  message += "👤 Victim Message:\n";
  message += "EMERGENCY!!! HELP ME\n\n";
  message += "🕒 Time: " + timestamp + "\n\n";
  message += "📍 Location:\n";
  message += "📍 GPS: " + String(latitude, 6) + ", " + String(longitude, 6) + "\n";
  message += "🔗 Google Maps: https://www.google.com/maps?q=" + String(latitude, 6) + "," + String(longitude, 6) + "\n\n";
  message += "📶 Signal: " + String(WiFi.RSSI()) + " dBm\n";
  message += "📱 Device: " + deviceName + "\n\n";
  message += "⚠ Please respond immediately!";

  sendTelegramMessageAsync(message);
  logEmergencyAlertAsync();
  
  Serial.println("✅ Emergency alert dispatched!");
  startLEDBlink(GREEN_LED);
}

void sendTelegramMessageAsync(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT);

  telegramDoc.clear();
  telegramDoc["chat_id"] = CHAT_ID;
  telegramDoc["text"] = message;
  telegramDoc["disable_web_page_preview"] = false;

  String payload;
  serializeJson(telegramDoc, payload);

  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("✅ Telegram message sent!");
  } else {
    Serial.printf("⚠ Telegram error: HTTP %d\n", httpCode);
  }

  http.end();
}

void logEmergencyAlertAsync() {
  if (WiFi.status() != WL_CONNECTED || !gpsLock) return;

  HTTPClient http;
  String url = String(FIREBASE_URL) + "/emergency_alerts.json";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT);

  firebaseDoc.clear();
  firebaseDoc["device_id"] = deviceId;
  firebaseDoc["device_name"] = deviceName;
  firebaseDoc["timestamp"] = getCurrentTimestamp();
  firebaseDoc["latitude"] = latitude;
  firebaseDoc["longitude"] = longitude;
  firebaseDoc["signal_strength"] = WiFi.RSSI();
  firebaseDoc["alert_type"] = "TRIPLE_TOUCH";
  firebaseDoc["status"] = "ACTIVE";

  String payload;
  serializeJson(firebaseDoc, payload);

  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("✅ Firebase alert logged!");
  } else {
    Serial.printf("❌ Firebase error: HTTP %d\n", httpCode);
  }

  http.end();
}

void logTouchEventAsync(int touchNumber) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(FIREBASE_URL) + "/touch_events.json";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT);

  firebaseDoc.clear();
  firebaseDoc["device_id"] = deviceId;
  firebaseDoc["timestamp"] = getCurrentTimestamp();
  firebaseDoc["touch_number"] = touchNumber;
  firebaseDoc["total_touches"] = totalTouches;

  String payload;
  serializeJson(firebaseDoc, payload);

  http.POST(payload);
  http.end();
}

void sendDeviceOnlineStatusAsync() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(FIREBASE_URL) + "/device_status/" + deviceId + ".json";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT);

  firebaseDoc.clear();
  firebaseDoc["device_id"] = deviceId;
  firebaseDoc["device_name"] = deviceName;
  firebaseDoc["status"] = "ONLINE";
  firebaseDoc["last_seen"] = getCurrentTimestamp();
  firebaseDoc["ip_address"] = WiFi.localIP().toString();
  firebaseDoc["signal_strength"] = WiFi.RSSI();
  firebaseDoc["total_touches"] = totalTouches;
  firebaseDoc["emergency_alerts"] = emergencyAlerts;
  firebaseDoc["gps_lock"] = gpsLock;

  String payload;
  serializeJson(firebaseDoc, payload);

  http.PUT(payload);
  http.end();
}

void sendDeviceOfflineStatusAsync() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(FIREBASE_URL) + "/device_status/" + deviceId + ".json";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_TIMEOUT);

  firebaseDoc.clear();
  firebaseDoc["device_id"] = deviceId;
  firebaseDoc["status"] = "OFFLINE";
  firebaseDoc["last_seen"] = getCurrentTimestamp();

  String payload;
  serializeJson(firebaseDoc, payload);

  http.PUT(payload);
  http.end();
}

void sendHeartbeatAsync() {
  sendDeviceOnlineStatusAsync();
}

String getCurrentTimestamp() {
  if (!timeInitialized) {
    return String(millis());
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String(millis());
  }
  
  char timeString[32];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}
