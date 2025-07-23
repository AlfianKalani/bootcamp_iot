#define BLYNK_TEMPLATE_ID "TMPL6yXmMTxox"
#define BLYNK_TEMPLATE_NAME "butkemiyot"
#define BLYNK_AUTH_TOKEN "L9K6VUWFwp5fdoI99ku39v84GmseqySD"
#define BLYNK_PRINT Serial

#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h> 
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

const char* wifiSsid = "Wokwi-GUEST";
const char* wifiPassword = "";
const char* firebaseProjectApiKey = "AIzaSyB6o0PdeUJbNRzV86SLifh0Jl3OrtBPmA4";
const char* firebaseDatabaseUrl ="https://bootcampiot-2f181-default-rtdb.asia-southeast1.firebasedatabase.app";

const char* dhtDataPath = "/dht_data";
const char* temperatureKey = "temperature";
const char* humidityKey = "humidity";
const char* ppmKey = "ppm";
const char* intensityKey = "intensity";

#define temperatureVpin V2
#define humidityVpin V3
#define noiseintensityVpin V5
#define ppmVpin V4
#define controlVpin V0
#define statusVpin V1

#define SOUND_PIN 33
#define PPM_PIN 32
#define DHT_PIN 12
#define LED_PIN 13
#define KIPAS_LED 14
#define TEMP_LED 16
#define HUM_LED 17
#define PPM_LED 18
#define NOISE_LED 19

BlynkTimer timer;
DHT dhtSensor(DHT_PIN, DHT22);
LiquidCrystal_I2C lcd(0x27, 20, 4);
bool activeStatus = false;
unsigned long lastSensorTime = 0;
unsigned long lastKipasTime = 0;
const unsigned long sensorInterval = 1000;
const unsigned long kipasInterval = 5000;
float ppm, sound, humidity, temperature;
FirebaseData dataMain;
FirebaseData dataStatus;
FirebaseData dataStream;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;
bool firebaseSignupOk = false;

void acquireData() {
    int16_t ppmValue = analogRead(PPM_PIN);
    ppm = ppmValue / 4.095; //0-1000

    int16_t soundValue = analogRead(SOUND_PIN);
    sound = 10 + ((float)soundValue / 4095.0) * 90; //10-100

    humidity = dhtSensor.readHumidity();
    temperature = dhtSensor.readTemperature();
}

void sendData() {
    Blynk.virtualWrite(humidityVpin, humidity);
    Blynk.virtualWrite(temperatureVpin, temperature);
    Blynk.virtualWrite(ppmVpin, ppm);
    Blynk.virtualWrite(noiseintensityVpin, sound);

        if (Firebase.ready() && firebaseSignupOk) {
            FirebaseJson jsonData;
            jsonData.set("humidity", humidity);
            jsonData.set("temperature", temperature);
            jsonData.set("PPM", ppm);
            jsonData.set("Noise", sound);

            if (Firebase.RTDB.pushJSON(&dataMain, dhtDataPath, &jsonData)) {
                char fullPath[128];
                char timestampPath[128];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", dataMain.dataPath().c_str(), dataMain.pushName().c_str());
                snprintf(timestampPath, sizeof(fullPath), "%s/%s", fullPath);
                Firebase.RTDB.setTimestamp(&dataMain, timestampPath);
                Serial.printf("Pushing data ok: %s\r\n", fullPath);
            } else {
                Serial.printf("Pushing data error: %s\r\n", dataMain.errorReason().c_str());
            }
        } else {
            Serial.println("Firebase not ready");
          }
}

void nyalaKipas() {
  unsigned long nowKipas = millis();

  if (nowKipas - lastKipasTime >= kipasInterval) {
    lastKipasTime = nowKipas;
    if (temperature > 24)
      digitalWrite(KIPAS_LED, HIGH);
    else digitalWrite(KIPAS_LED, LOW);
  }
}

void nyalaLED() {
  if(temperature > 24 || temperature < 20)
    digitalWrite(TEMP_LED, HIGH);
  else digitalWrite(TEMP_LED, LOW);
  if(humidity > 60 || humidity < 40)
    digitalWrite(HUM_LED, HIGH);
  else digitalWrite(HUM_LED, LOW);
  if(ppm > 700)
    digitalWrite(PPM_LED, HIGH);
  else digitalWrite(PPM_LED, LOW);
  if(sound > 45)
    digitalWrite(NOISE_LED, HIGH);
  else digitalWrite(NOISE_LED, LOW);
}

void tampilkanLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature, 1);
  lcd.print((char)223); // simbol derajat
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Hum : ");
  lcd.print(humidity, 1);
  lcd.print("%    ");

  lcd.setCursor(0, 2);
  lcd.print("PPM : ");
  lcd.print(ppm, 0);
  lcd.print("      ");

  lcd.setCursor(0, 3);
  lcd.print("Noise: ");
  lcd.print(sound, 0);
  lcd.print(" dB  ");
}

void showStatus(bool status) {
    String statusString;
    if (status) {
        digitalWrite(LED_PIN, HIGH);
        statusString = "ON";
    } else {
        digitalWrite(LED_PIN, LOW);
        statusString = "OFF";
    }
    Serial.printf("Status: %s\r\n", statusString.c_str());

    Blynk.virtualWrite(statusVpin, status);


    if (Firebase.ready() && firebaseSignupOk) {
        if (Firebase.RTDB.setBool(&dataStatus, temperature,status)) {
            Serial.println("Pushing status ok");
        } else {
            Serial.printf("Pushing status error: %s\r\n", dataStatus.errorReason().c_str());
        }
    } else {
        Serial.println("Firebase not ready");
    }
}

BLYNK_CONNECTED() {
    Serial.println("Blynk connected");
    Blynk.syncVirtual(controlVpin);
}

BLYNK_WRITE(controlVpin) {
    Serial.printf("Control pin value: %s\r\n", param);
    int pinValue = param.asInt();
    if (pinValue == 1) {
        activeStatus = true;
    } else {
        activeStatus = false;
    }
    showStatus(activeStatus);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(TEMP_LED, OUTPUT);
  pinMode(HUM_LED, OUTPUT);
  pinMode(PPM_LED, OUTPUT);
  pinMode(NOISE_LED, OUTPUT);
  pinMode(KIPAS_LED, OUTPUT);
  Serial.begin(115200);
  Serial.println("Hello, ESP32!");
  Wire.begin(23, 22);
  dhtSensor.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(7, 1);
  lcd.print("TB+Nia");
  lcd.setCursor(1, 2);
  lcd.print("Monitor Ruang UGD");

  Blynk.begin(BLYNK_AUTH_TOKEN, wifiSsid, wifiPassword);

  WiFi.mode(WIFI_STA);
      WiFi.begin(wifiSsid, wifiPassword);
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
      }
    Serial.printf("\r\nWiFi connected, IP address: %s\r\n", WiFi.localIP().toString().c_str());

    Serial.print("Firebase Signup ... ");
    firebaseConfig.api_key = firebaseProjectApiKey;
    firebaseConfig.database_url = firebaseDatabaseUrl;
    if (Firebase.signUp(&firebaseConfig, &firebaseAuth, "", "")) {
        firebaseSignupOk = true;
        Serial.println("ok");
    } else {
        Serial.println(firebaseConfig.signer.signupError.message.c_str());
    }
    firebaseConfig.token_status_callback = tokenStatusCallback;
      
      Firebase.begin(&firebaseConfig, &firebaseAuth);
      Firebase.reconnectWiFi(true);

      Serial.print("Firebase Stream ... ");
      dataStream.keepAlive(5, 5, 1);
      if (Firebase.RTDB.beginStream(&dataStream, dhtDataPath)) {
          Serial.println("ok");
      } else {
          Serial.println(dataStream.errorReason());
      }

  lcd.clear();
}

void loop() {
  Blynk.run();
  if(activeStatus) {
    unsigned long now = millis();

    acquireData();
    nyalaKipas();
    nyalaLED();
    if (now - lastSensorTime >= sensorInterval) {
      lastSensorTime = now;
      sendData();
      tampilkanLCD();
    }
  }
}