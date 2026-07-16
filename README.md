# 🛡️ IoT-Based Women Safety Device

An ESP32-based wearable emergency alert system designed to improve women's safety through real-time location tracking and cloud-based notifications. The device detects an SOS trigger, retrieves the user's live GPS location, sends emergency alerts via Telegram, and logs all events to Firebase for monitoring.

---

## 🚀 Features

- Triple-touch SOS activation
- Live GPS location using NEO-6M
- Telegram Bot emergency alerts
- Firebase Firestore event logging
- Device status monitoring
- LED indicators for system status
- Rechargeable battery-powered design

---

## 🛠️ Hardware Components

- ESP32 Dev Board
- NEO-6M GPS Module
- TTP223 Touch Sensor
- LEDs (Red, Yellow, Green)
- TP4056 Charging Module
- 3.7V Li-ion Battery

---

## 💻 Software Stack

- Arduino IDE
- ESP32 WiFi Library
- TinyGPS++
- Firebase ESP Client
- Universal Telegram Bot
- ArduinoJson

---

## 📂 Project Workflow

```text
Touch Sensor
      │
      ▼
Detect Triple Touch
      │
      ▼
Read GPS Coordinates
      │
      ▼
Connect to Wi-Fi
      │
      ▼
Send Telegram Alert
      │
      ▼
Store Event in Firebase
      │
      ▼
Update Device Status
```

---

## 📌 Key Functionalities

### Triple Touch Detection

```cpp
if (touchCount >= 3) {
    triggerEmergency();
}
```

---

### Reading GPS Location

```cpp
while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
}

latitude = gps.location.lat();
longitude = gps.location.lng();
```

---

### Sending Telegram Alert

```cpp
String message =
"🚨 Emergency Alert!\n" +
"Location:\nhttps://maps.google.com/?q=" +
String(latitude,6) + "," + String(longitude,6);

bot.sendMessage(CHAT_ID, message, "");
```

---

### Uploading Data to Firebase

```cpp
FirebaseJson content;

content.set("fields/status/stringValue", "Emergency");
content.set("fields/latitude/doubleValue", latitude);
content.set("fields/longitude/doubleValue", longitude);
```

---

## 📊 Firebase Collections

```
device_status
touch_events
emergency_alerts
```

Each emergency event stores:

- Timestamp
- GPS Coordinates
- Device Status
- Touch Count
- Alert Information

---

## 📷 System Architecture

```
Touch Sensor
      │
      ▼
    ESP32
      │
 ┌────┴─────┐
 │          │
 ▼          ▼
GPS      Firebase
 │          │
 ▼          ▼
Telegram   Dashboard
```

---

## 🎯 Applications

- Women's Safety
- Personal Emergency Response
- Elderly Monitoring
- Child Safety
- Lone Worker Protection

---

## 🔮 Future Enhancements

- GSM support for offline messaging
- Mobile application
- Voice activation
- Fall detection
- SOS wearable integration
- Live tracking dashboard

---

## 👩‍💻 Author

**Nithya Anvitha Thumma**

B.Tech – Artificial Intelligence and Data Science

Amrita Vishwa Vidyapeetham
