#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define clapDelay 1000
int clapCount = 0;
unsigned long lastClapTime = 0; // Thời gian vỗ tay lần cuối

// WiFi và Firebase thông tin
#define WIFI_SSID "P106B"
#define WIFI_PASSWORD "hoilamgi"
#define API_KEY "AIzaSyD94jtSeJF-F1pHvp9SqJn8gBYZejxJ3CQ"
#define DATABASE_URL "https://soniclight-5e4be-default-rtdb.asia-southeast1.firebasedatabase.app/" 

// GPIO cho relay và microphone
#define RELAY_PIN 12      // Chân relay
#define MIC_PIN 19        // Chân microphone

// Firebase và cảm biến
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
bool relayState = false;   // Đổi tên biến từ ledState thành relayState

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Đặt relay ban đầu ở trạng thái tắt

  pinMode(MIC_PIN, INPUT);  // Thiết lập chân microphone là INPUT

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Sign-Up OK");
    signupOK = true;
  } else {
    Serial.printf("Firebase Sign-Up Error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Kiểm tra Firebase
  if (Firebase.ready() && signupOK) {
    // Đọc trạng thái relay từ Firebase
    if (Firebase.RTDB.getBool(&fbdo, "control/relay")) {
      bool firebaseRelayState = fbdo.boolData();
      if (firebaseRelayState != relayState) {
        relayState = firebaseRelayState;
        digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
       // Serial.println(relayState ? "Relay bật từ Firebase" : "Relay tắt từ Firebase");
      }
    } else {
      Serial.println(fbdo.errorReason());
    }
  }

  // Kiểm tra tiếng vỗ tay từ microphone
  bool micValue = digitalRead(MIC_PIN);  // Đọc giá trị từ microphone (nhị phân: HIGH hoặc LOW)
  //Serial.println(micValue);

  // Kiểm tra nếu có âm thanh đủ lớn (vỗ tay)
  if (micValue == LOW) {  // Giả sử LOW nghĩa là có âm thanh
    unsigned long currentTime = millis();
    if (currentTime - lastClapTime <= clapDelay && clapCount > 0) {
      clapCount++;  // Tăng số lần vỗ tay
      lastClapTime = currentTime;

      if (clapCount == 2) {  // Kiểm tra nếu vỗ tay 2 lần liên tiếp
        relayState = !relayState;  // Chuyển đổi trạng thái relay
        digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
        //Serial.println(relayState ? "Relay bật từ microphone" : "Relay tắt từ microphone");
        
        // Cập nhật trạng thái lên Firebase
        if (Firebase.RTDB.setBool(&fbdo, "control/relay", relayState)) {
          Serial.println("Cập nhật trạng thái relay lên Firebase thành công");
        } else {
          Serial.println(fbdo.errorReason());
        }

        // Đặt lại số lần vỗ tay
        clapCount = 0;
      }
    } else if (currentTime - lastClapTime > clapDelay) {
      // Đặt lại số lần vỗ tay nếu thời gian giữa hai lần vỗ tay vượt quá clapDelay
      clapCount = 1;
      lastClapTime = currentTime;
    }
  }

  delay(50);  // Giảm thời gian delay để xử lý tín hiệu nhanh hơn
}