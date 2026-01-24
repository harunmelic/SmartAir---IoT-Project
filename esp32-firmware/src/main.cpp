#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "wifi_config.h"
#include "firebase_config.h"

// PIR senzor (Motion Detection)
#define PIR_PIN 13       // D13 - PIR senzor OUT pin
#define BUZZER_PIN 4     // D4 - Buzzer pin
#define LED_PIN 2        // D2 - Built-in LED

// LEDC za buzzer (ESP32 nema tone() funkciju)
#define BUZZER_CHANNEL 0
#define BUZZER_RESOLUTION 8
#define BUZZER_FREQUENCY 2000

// NTP Server za timestamp
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;  // GMT+1 (Bosna)
const int daylightOffset_sec = 3600;  // Ljetno vrijeme

// PIR debounce
#define PIR_DEBOUNCE_TIME 2000  // 2 sekunde prije novog triggera
unsigned long lastMotionTime = 0;

// Alarm timeout (30 sekundi)
#define ALARM_TIMEOUT 30000
unsigned long alarmStartTime = 0;
bool alarmActive = false;

// Firebase objekti
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastReconnectAttempt = 0;
unsigned long lastDataSend = 0;
bool firebaseReady = false;
bool isArmed = false;                // Da li je alarm aktivan
bool motionDetected = false;         // Da li je detektovan pokret
unsigned long lastArmCheck = 0;      // Vrijeme posljednje provjere arm statusa
bool ntpSynced = false;              // Da li je NTP sinhronizovan

// Planirani period (schedule)
bool scheduleEnabled = false;
int scheduleStartHour = 22;          // Početak sat (22:00)
int scheduleStartMinute = 0;         // Početak minut
int scheduleEndHour = 6;             // Kraj sat (06:00)
int scheduleEndMinute = 0;           // Kraj minut
unsigned long lastScheduleCheck = 0;
// WiFi konekcija
void connectToWiFi() {
  Serial.println("Povezivanje na WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi povezan!");
    Serial.print("IP adresa: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNeuspjelo povezivanje na WiFi.");
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
      Serial.println("WiFi disconnected. Pokušaj ponovnog povezivanja...");
      lastReconnectAttempt = currentMillis;
      connectToWiFi();
    }
  }
}

// Firebase inicijalizacija
void initFirebase() {
  Serial.println("Initializing Firebase...");
  
  config.api_key = FIREBASE_AUTH;
  config.database_url = FIREBASE_HOST;
  
  auth.user.email = FIREBASE_USER_EMAIL;
  auth.user.password = FIREBASE_USER_PASSWORD;
  
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("Firebase initialized!");
}

// Šalje podatke na Firebase (boolean)
void sendBool(String path, bool value) {
  if (!firebaseReady) return;
  
  String fullPath = "devices/" + String(DEVICE_ID) + "/" + path;
  
  if (Firebase.RTDB.setBool(&fbdo, fullPath.c_str(), value)) {
    Serial.println("Podaci poslani: " + fullPath + " = " + String(value ? "true" : "false"));
  } else {
    Serial.println("Greska prilikom slanja podataka: " + fbdo.errorReason());
  }
}

// Šalje podatke na Firebase (int)
void sendInt(String path, int value) {
  if (!firebaseReady) return;
  
  String fullPath = "devices/" + String(DEVICE_ID) + "/" + path;
  
  if (Firebase.RTDB.setInt(&fbdo, fullPath.c_str(), value)) {
    Serial.println("Podaci poslani: " + fullPath + " = " + String(value));
  } else {
    Serial.println("Greska prilikom slanja podataka: " + fbdo.errorReason());
  }
}

// Čita boolean komandu iz Firebase
bool readBoolCommand(String path) {
  if (!firebaseReady) return false;
  
  String fullPath = "devices/" + String(DEVICE_ID) + "/" + path;
  
  if (Firebase.RTDB.getBool(&fbdo, fullPath.c_str())) {
    bool command = fbdo.boolData();
    Serial.println("Komanda pročitana: " + fullPath + " = " + String(command ? "true" : "false"));
    return command;
  } else {
    Serial.println("Neuspjelo čitanje komande: " + fbdo.errorReason());
    return false;
  }
}

// Šalje alert (hitnu poruku)
void sendAlert(String message) {
  if (!firebaseReady) {
    Serial.println("⚠️ Ne mogu poslati alert - Firebase nije spreman");
    return;
  }
  
  String alertPath = "devices/" + String(DEVICE_ID) + "/alerts/last";
  
  if (Firebase.RTDB.setString(&fbdo, alertPath.c_str(), message)) {
    Serial.println("Alert sent: " + message);
  } else {
    Serial.println("Greška prilikom slanja alerta: " + fbdo.errorReason());
  }
}

// Čitaj schedule podešavanja iz Firebase
void readScheduleSettings() {
  if (!firebaseReady) return;
  
  String enabledPath = "devices/" + String(DEVICE_ID) + "/schedule/enabled";
  String startHourPath = "devices/" + String(DEVICE_ID) + "/schedule/startHour";
  String startMinutePath = "devices/" + String(DEVICE_ID) + "/schedule/startMinute";
  String endHourPath = "devices/" + String(DEVICE_ID) + "/schedule/endHour";
  String endMinutePath = "devices/" + String(DEVICE_ID) + "/schedule/endMinute";
  
  if (Firebase.RTDB.getBool(&fbdo, enabledPath.c_str())) {
    scheduleEnabled = fbdo.boolData();
  }
  
  if (Firebase.RTDB.getInt(&fbdo, startHourPath.c_str())) {
    scheduleStartHour = fbdo.intData();
  }
  
  if (Firebase.RTDB.getInt(&fbdo, startMinutePath.c_str())) {
    scheduleStartMinute = fbdo.intData();
  }
  
  if (Firebase.RTDB.getInt(&fbdo, endHourPath.c_str())) {
    scheduleEndHour = fbdo.intData();
  }
  
  if (Firebase.RTDB.getInt(&fbdo, endMinutePath.c_str())) {
    scheduleEndMinute = fbdo.intData();
  }
  
  Serial.print("Schedule loaded: enabled=");
  Serial.print(scheduleEnabled ? "YES" : "NO");
  Serial.print(", period=");
  Serial.print(scheduleStartHour);
  Serial.print(":");
  if (scheduleStartMinute < 10) Serial.print("0");
  Serial.print(scheduleStartMinute);
  Serial.print(" - ");
  Serial.print(scheduleEndHour);
  Serial.print(":");
  if (scheduleEndMinute < 10) Serial.print("0");
  Serial.println(scheduleEndMinute);
}

// Provjeri da li je trenutno vrijeme u planiranom periodu
bool isInSchedulePeriod() {
  if (!scheduleEnabled) return false;
  
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  
  // Konvertuj vrijeme u minute (od ponoći)
  int currentTotalMinutes = currentHour * 60 + currentMinute;
  int startTotalMinutes = scheduleStartHour * 60 + scheduleStartMinute;
  int endTotalMinutes = scheduleEndHour * 60 + scheduleEndMinute;
  
  // DEBUG: Ispiši trenutno vrijeme
  Serial.print("🕐 Trenutno vrijeme: ");
  Serial.print(currentHour);
  Serial.print(":");
  if (currentMinute < 10) Serial.print("0");
  Serial.print(currentMinute);
  Serial.print(" | Schedule: ");
  Serial.print(scheduleStartHour);
  Serial.print(":");
  if (scheduleStartMinute < 10) Serial.print("0");
  Serial.print(scheduleStartMinute);
  Serial.print(" - ");
  Serial.print(scheduleEndHour);
  Serial.print(":");
  if (scheduleEndMinute < 10) Serial.print("0");
  Serial.print(scheduleEndMinute);
  
  bool inPeriod = false;
  
  // Ako je period preko ponoći (npr. 22:30 - 06:15)
  if (startTotalMinutes > endTotalMinutes) {
    inPeriod = (currentTotalMinutes >= startTotalMinutes || currentTotalMinutes < endTotalMinutes);
  } else {
    // Normalan period (npr. 08:30 - 18:45)
    inPeriod = (currentTotalMinutes >= startTotalMinutes && currentTotalMinutes < endTotalMinutes);
  }
  
  Serial.print(" | U periodu: ");
  Serial.println(inPeriod ? "DA ✅" : "NE ❌");
  
  return inPeriod;
}
bool verifyPinCommand(bool &armCommand) {
  if (!firebaseReady) return false;
  
  String commandPath = "devices/" + String(DEVICE_ID) + "/commands";
  
  // Provjeri da li komanda postoji
  if (Firebase.RTDB.get(&fbdo, commandPath.c_str())) {
    if (fbdo.dataType() == "json") {
      FirebaseJson json = fbdo.jsonObject();
      FirebaseJsonData pinData;
      FirebaseJsonData actionData;
      
      // Čitaj PIN i action iz JSON objekta
      json.get(pinData, "pin");
      json.get(actionData, "action");
      
      if (pinData.success && actionData.success) {
        String receivedPin = pinData.stringValue;
        bool action = actionData.boolValue;
        
        // Verifikuj PIN
        if (receivedPin == String(ALARM_PIN)) {
          Serial.println("✅ PIN VERIFIED - Action: " + String(action ? "ARM" : "DISARM"));
          armCommand = action;
          
          // Obriši komandu nakon izvršenja
          Firebase.RTDB.deleteNode(&fbdo, commandPath.c_str());
          return true;
        } else {
          Serial.println("❌ PIN INCORRECT: " + receivedPin);
          sendAlert("⚠️ Incorrect PIN code!");
          
          // Obriši netačnu komandu
          Firebase.RTDB.deleteNode(&fbdo, commandPath.c_str());
          return false;
        }
      }
    }
  }
  
  return false;
}

// Log pokreta (motion event) sa timestamp-om
void logMotionEvent() {
  if (!firebaseReady) return;
  
  // Koristi pravi epoch timestamp sa NTP
  time_t now;
  time(&now);
  
  String logPath = "devices/" + String(DEVICE_ID) + "/motionLogs/" + String(now);
  String timestampPath = "devices/" + String(DEVICE_ID) + "/lastMotion";
  
  if (Firebase.RTDB.setString(&fbdo, logPath.c_str(), "Motion detected")) {
    Firebase.RTDB.setInt(&fbdo, timestampPath.c_str(), now * 1000); // Milisekunde za JavaScript Date
    Serial.print("Motion event logged at: ");
    Serial.println(now);
  } else {
    Serial.println("Failed to log motion: " + fbdo.errorReason());
  }
}



// Buzzer confirmation (3 sec kada se ARM-uje alarm)
void playConfirmationBeep() {
  // Koristi LEDC za tonove
  ledcWriteTone(BUZZER_CHANNEL, 2000);
  delay(200);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  delay(200);
  ledcWriteTone(BUZZER_CHANNEL, 2000);
  delay(200);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  delay(200);
  ledcWriteTone(BUZZER_CHANNEL, 2500);
  delay(400);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  
  Serial.println("Confirmation beep played!");
}

// Alarm triggered buzzer (siren pattern)
void triggerAlarm() {
  if (!alarmActive) {
    alarmActive = true;
    alarmStartTime = millis();
    digitalWrite(LED_PIN, HIGH);  // Upali LED
    Serial.println("🚨 ALARM TRIGGERED! 🚨");
  }
  
  // Siren pattern - alternira između visoke i niske frekvencije
  unsigned long currentTime = millis();
  int cycleTime = (currentTime / 200) % 4;  // 4 faze po 200ms
  
  if (cycleTime == 0 || cycleTime == 2) {
    ledcWriteTone(BUZZER_CHANNEL, 2500);  // Visoka frekvencija
  } else if (cycleTime == 1 || cycleTime == 3) {
    ledcWriteTone(BUZZER_CHANNEL, 1500);  // Niska frekvencija
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 Pametni Alarm se pokreće...");
  
  // Inicijalizuj PIR senzor
  pinMode(PIR_PIN, INPUT);
  Serial.println("PIR sensor initialized on pin D13!");
  
  // Inicijalizuj LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Počni sa LED off
  Serial.println("LED initialized on pin D2!");
  
  // Inicijalizuj Buzzer sa LEDC (PWM) za glasniji zvuk
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  ledcWrite(BUZZER_CHANNEL, 0);  // Počni sa buzzer off
  Serial.println("Buzzer initialized on pin D4 with LEDC!");
  
  // Test buzzer (kratki beep pri startu)
  ledcWriteTone(BUZZER_CHANNEL, 2000);
  delay(100);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  Serial.println("Buzzer test completed!");
  
  // Povezica na WiFi
  connectToWiFi();
  
  // Inicijalizuj Firebase
  if (WiFi.status() == WL_CONNECTED) {
    initFirebase();
    
    // Inicijalizuj NTP za pravi timestamp
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("NTP sinhronizacija pokrenuta!");
  }
}

void loop() {
  checkWiFiConnection();
  
  if (Firebase.ready()) {
    if (!firebaseReady) {
      firebaseReady = true;
      Serial.println("Firebase je spreman!");
      sendAlert("Smart Alarm system connected and ready!");
      
      // Učitaj schedule podešavanja pri startu
      readScheduleSettings();
    }
    
    // Provjeri schedule svakih 10 sekundi (za brže testiranje)
    if (millis() - lastScheduleCheck > 10000) {
      lastScheduleCheck = millis();
      
      // Refresh schedule podešavanja
      readScheduleSettings();
      
      // Samo ako je schedule enabled, provjeri period
      if (scheduleEnabled) {
        bool shouldBeArmed = isInSchedulePeriod();
        
        // Automatski ARM ako je u planiranom periodu i nije već ARM-ovan
        if (shouldBeArmed && !isArmed) {
          Serial.println("⏰ AUTO-ARM: Ušli u planirani period!");
          isArmed = true;
          playConfirmationBeep();
          sendBool("status/armed", true);
          sendAlert("⏰ Alarm AUTO-ARMED - scheduled period activated");
        }
        
        // Automatski DISARM SAMO ako schedule izlazi iz perioda (ne ako korisnik manuelno disarmuje)
        // Provjeri da li je već bio armed zbog schedule-a
        if (!shouldBeArmed && isArmed) {
          Serial.println("⏰ AUTO-DISARM: Izašli iz planiranog perioda!");
          digitalWrite(BUZZER_PIN, LOW);
          alarmActive = false;
          isArmed = false;
          sendBool("status/triggered", false);
          sendBool("status/armed", false);
          sendAlert("⏰ Alarm AUTO-DISARMED - scheduled period ended");
        }
      }
    }
    
    // Proveri ARM status direktno iz Firebase (jednostavno rešenje)
    if (millis() - lastArmCheck > 2000) {
      lastArmCheck = millis();
      
      String armedPath = "devices/" + String(DEVICE_ID) + "/status/armed";
      if (Firebase.RTDB.getBool(&fbdo, armedPath.c_str())) {
        bool newArmedStatus = fbdo.boolData();
        
        // Ako se ARM status promijenio
        if (newArmedStatus && !isArmed) {
          Serial.println("✅ ALARM ARMED!");
          playConfirmationBeep();
          isArmed = true;
          sendAlert("Alarm ARMED - system active");
        }
        
        // Ako se DISARM-uje
        if (!newArmedStatus && isArmed) {
          Serial.println("⛔ ALARM DISARMED!");
          digitalWrite(BUZZER_PIN, LOW);
          alarmActive = false;
          sendBool("status/triggered", false);
          isArmed = false;
          sendAlert("Alarm DISARMED - system inactive");
        }
      }
    }
    // Provjeri ARM/DISARM komandu sa PIN verifikacijom
    bool armCommand = false;
    if (verifyPinCommand(armCommand)) {
      if (armCommand != isArmed) {
        isArmed = armCommand;
        
        if (isArmed) {
          playConfirmationBeep();
          Serial.println("🔒 Alarm ARMED - sistem spreman!");
          sendBool("status/armed", true);
          sendAlert("Alarm ARMED - system ready!");
        } else {
          Serial.println("🔓 Alarm DISARMED");
          sendBool("status/armed", false);
          sendBool("status/triggered", false);
          ledcWriteTone(BUZZER_CHANNEL, 0);  // Zaustavi buzzer
          digitalWrite(LED_PIN, LOW);  // Ugasi LED
          alarmActive = false;
          sendAlert("Alarm DISARMED - system inactive");
        }
      }
    }
    
    // Čitaj PIR senzor kontinuirano ako je alarm ARM-ovan
    if (isArmed) {
      int pirState = digitalRead(PIR_PIN);
      
      // Debounce: provjeri je li prošlo dovoljno vremena od zadnjeg pokreta
      if (pirState == HIGH && !motionDetected && (millis() - lastMotionTime > PIR_DEBOUNCE_TIME)) {
        motionDetected = true;
        lastMotionTime = millis();
        Serial.println("🚨 POKRET DETEKTOVAN!");
        
        // Aktiviraj alarm
        triggerAlarm();
        sendBool("status/triggered", true);
        sendAlert("⚠️ ALARM TRIGGERED - Motion detected!");
        logMotionEvent();
        
      } else if (pirState == LOW && motionDetected) {
        motionDetected = false;
        Serial.println("Pokret prestao");
      }
      
      // Nastavi pulsiranje alarma dok je aktivan
      if (alarmActive) {
        // Provjeri timeout (30 sekundi)
        if (millis() - alarmStartTime > ALARM_TIMEOUT) {
          alarmActive = false;
          ledcWriteTone(BUZZER_CHANNEL, 0);  // Zaustavi buzzer
          digitalWrite(LED_PIN, LOW);  // Ugasi LED
          sendBool("status/triggered", false);
          Serial.println("⏱️ Alarm timeout - buzzer zaustavljen");
        } else {
          triggerAlarm();  // Nastavi pulsiranje
        }
      }
    } else {
      // Ako nije ARM-ovan, zaustavi buzzer i resetuj detekciju
      ledcWriteTone(BUZZER_CHANNEL, 0);  // Zaustavi buzzer
      digitalWrite(LED_PIN, LOW);  // Ugasi LED
      motionDetected = false;
      alarmActive = false;
    }
    
    // Šalji status svakih 10 sekundi
    if (millis() - lastDataSend > 10000) {
      lastDataSend = millis();
      
      sendBool("status/armed", isArmed);
      sendInt("sensor/pir", digitalRead(PIR_PIN));
      
      Serial.print("Status update - Armed: ");
      Serial.print(isArmed ? "YES" : "NO");
      Serial.print(" | PIR: ");
      Serial.println(digitalRead(PIR_PIN) ? "HIGH" : "LOW");
    }
  }
  
  delay(100);
}