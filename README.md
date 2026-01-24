# Pametni Alarm - IoT Projekat

Pametni IoT alarm sistem baziran na ESP32 mikrokontroleru za detekciju pokreta i kontrolu preko web aplikacije.

##  Opis

Sistem koristi PIR senzor za detekciju pokreta, buzzer za audio signalizaciju, i Firebase Realtime Database za IoT komunikaciju. Kontrola se vrši preko Angular web aplikacije sa funkcionalnostima ARM/DISARM alarma, praćenjem logova pokreta, i real-time notifikacijama.

##  Komponente

### Hardware:
- **ESP32 Dev Board**
- **PIR Senzor** (GPIO 13)
- **Buzzer** (GPIO 4)

### Software:
- **ESP32 Firmware** (PlatformIO/Arduino)
- **Angular Web App** (Angular 18)
- **Firebase Realtime Database**

##  Struktura:

```
Pametni-Alarm/
├── esp32-firmware/    # ESP32 kod (PlatformIO)
│   ├── src/
│   │   └── main.cpp
│   ├── include/
│   │   ├── firebase_config.h
│   │   └── wifi_config.h
│   └── platformio.ini
├── angular-app/       # Angular frontend
│   ├── src/
│   │   ├── app/
│   │   │   ├── components/
│   │   │   └── services/
│   │   └── environments/
│   └── package.json
└── README.md
```

##  Pokretanje

### ESP32 Firmware:
1. Otvori `esp32-firmware` folder u PlatformIO (VS Code Extension)
2. Podesi WiFi kredencijale u `include/wifi_config.h`:
   ```cpp
   #define WIFI_SSID "tvoja_wifi_mreza"
   #define WIFI_PASSWORD "tvoja_lozinka"
   ```
3. Podesi Firebase kredencijale u `include/firebase_config.h`:
   ```cpp
   #define FIREBASE_HOST "tvoj-projekat.firebasedatabase.app"
   #define FIREBASE_AUTH "tvoj_api_key"
   #define FIREBASE_USER_EMAIL "admin@email.com"
   #define FIREBASE_USER_PASSWORD "password"
   ```
4. Build & Upload na ESP32 (PlatformIO: Upload)

### Angular App:
```bash
cd angular-app
npm install --legacy-peer-deps
npm start
```
Aplikacija će biti dostupna na: http://localhost:4200

### Firebase Setup:
1. Kreiraj Firebase projekat na https://console.firebase.google.com
2. Omogući Realtime Database (Europe-West1 region)
3. Podesi Database Rules:
   ```json
   {
     "rules": {
       "devices": {
         ".read": "auth != null",
         ".write": "auth != null"
       }
     }
   }
   ```
4. Kreiraj Authentication Email/Password korisnika
5. Kopiraj kredencijale u config fajlove

## 🔧 Funkcionalnosti:

-  PIR detekcija pokreta sa debounce-om (2s)
-  ARM/DISARM alarm kontrola
-  Buzzer potvrda aktivacije (3 beep)
-  Alarm timeout (30 sekundi automatsko zaustavljanje)
-  NTP sinhronizacija za pravi timestamp
-  Real-time Firebase sinhronizacija
-  Web kontrolni panel (Angular 18)
-  Log pokreta sa vremenskim pečatom
-  Alert notifikacije
-  Connection status monitoring

##  Firebase Struktura:

```
devices/
  └── alarm_esp32_main/
      ├── status/
      │   ├── armed: boolean
      │   └── triggered: boolean
      ├── sensor/
      │   └── pir: 0 | 1
      ├── alerts/
      │   ├── last: string
      │   └── timestamp: number
      ├── motionLogs/
      │   └── [timestamp]: "Motion detected"
      └── lastMotion: timestamp
```

##  Sigurnost:

- Firebase Authentication obavezna
- WiFi WPA2 enkripcija
- Environment variables za sensitive podatke
- HTTPS komunikacija

##  Napomene:

- ESP32 mora biti povezan na WiFi mrežu
- Firebase projekat mora biti aktivan
- PIR senzor reaguje na pokret u radijusu ~7m
- Buzzer može biti aktivan ili pasivni (5V)
- NTP server: pool.ntp.org (GMT+1 Bosna)

## 🔧 Troubleshooting:

**ESP32 se ne povezuje na WiFi:**
- Provjeri SSID i password u wifi_config.h
- Provjeri jačinu WiFi signala
- Resetuj ESP32

**Firebase greške:**
- Provjeri API key i Database URL
- Provjeri Authentication kredencijale
- Provjeri Database Rules

**Alarm ne reaguje:**
- Provjeri PIR senzor konekciju (GPIO 13)
- Provjeri da li je alarm ARM-ovan
- Provjeri Serial Monitor za debug output

##  Licenca:

Open-source projekat za edukativne svrhe.
