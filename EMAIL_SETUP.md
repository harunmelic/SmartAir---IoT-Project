# SmartAir Email Notifikacije - Setup

## 1. Instalacija Firebase Functions

```bash
cd functions
npm install
```

## 2. Gmail App Password Setup

1. Idi na Google Account: https://myaccount.google.com/
2. Security → 2-Step Verification (mora biti uključeno)
3. App passwords → Generate new app password
4. Odaberi "Mail" i "Other device"
5. Kopiraj generirani password (16 karaktera)

## 3. Konfiguracija Firebase Functions

```bash
# Postavi environment variable u Firebase
firebase functions:config:set email.user="vas.email@gmail.com"
firebase functions:config:set email.password="your-16-char-app-password"
```

## 4. Deploy Functions

```bash
cd functions
firebase deploy --only functions
```

## 5. Kako Funkcionise

- ESP32 šalje alert u Firebase `/alerts/last`
- Cloud Function detektuje promenu
- Provera: Da li je prošlo 5 minuta od zadnjeg email-a
- Ako DA → Šalje email na adresu iz `/settings/notificationEmail`
- Ako NE → Preskače (rate limit)

## 6. Test

1. Unesi svoj email u Angular dashboard
2. Klikni "Sačuvaj"
3. Triggeruj alarm
4. Proveri inbox (i spam folder)

## 7. Troubleshooting

**Email se ne šalje:**
- Provjeri da li je App Password tačan
- Provjeri Firebase Console → Functions logs
- Provjeri spam folder

**Rate limit:**
- Email se šalje MAX jednom u 5 minuta
- Provjeri `/settings/lastEmailSent` timestamp u Firebase
