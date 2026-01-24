const functions = require('firebase-functions');
const admin = require('firebase-admin');
const nodemailer = require('nodemailer');
require('dotenv').config();

admin.initializeApp();

// Email konfiguracija (koristi environment variable)
// Gmail SMTP - potrebno je omogućiti "App Password" u Gmail settings
const transporter = nodemailer.createTransport({
  service: 'gmail',
  auth: {
    user: process.env.EMAIL_USER || 'harun1melic6@gmail.com',
    pass: process.env.EMAIL_PASSWORD || 'ijjbkfajockghduk'
  }
});

// Rate limiting - šalje max 1 email na 5 minuta
const RATE_LIMIT_MS = 5 * 60 * 1000; // 5 minuta

exports.sendAlarmEmail = functions.database
  .ref('/devices/alarm_esp32_main/alerts/last')
  .onUpdate(async (change, context) => {
    const alertMessage = change.after.val();
    
    if (!alertMessage) {
      console.log('No alert message');
      return null;
    }

    // Šalji email SAMO ako je alarm triggerovan (pokret detektovan)
    if (!alertMessage.includes('TRIGGERED') && !alertMessage.includes('Pokret detektovan')) {
      console.log('Not a triggered alert, skipping email. Message:', alertMessage);
      return null;
    }

    try {
      // Dohvati email adresu iz settings
      const emailSnapshot = await admin.database()
        .ref('/devices/alarm_esp32_main/settings/notificationEmail')
        .once('value');
      
      const recipientEmail = emailSnapshot.val();
      
      if (!recipientEmail) {
        console.log('No email configured for notifications');
        return null;
      }

      // Provjeri rate limit
      const lastEmailSnapshot = await admin.database()
        .ref('/devices/alarm_esp32_main/settings/lastEmailSent')
        .once('value');
      
      const lastEmailTime = lastEmailSnapshot.val() || 0;
      const now = Date.now();
      
      if (now - lastEmailTime < RATE_LIMIT_MS) {
        console.log(`Rate limit active. Last email sent ${Math.floor((now - lastEmailTime) / 1000)}s ago`);
        return null;
      }

      // Pripremi email
      const mailOptions = {
        from: `SmartAlarm <${process.env.EMAIL_USER || 'harun1melic6@gmail.com'}>`,
        to: recipientEmail,
        subject: '🚨 SmartAlarm - Upozorenje!',
        html: `
          <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto;">
            <div style="background: #f44336; color: white; padding: 20px; text-align: center;">
              <h1>🚨 ALARM TRIGGERED!</h1>
            </div>
            <div style="padding: 20px; background: #f5f5f5;">
              <h2>Detalji alarma:</h2>
              <p style="font-size: 16px;"><strong>${alertMessage}</strong></p>
              <p style="color: #666;">Vrijeme: ${new Date().toLocaleString('sr-RS')}</p>
              <hr style="border: 1px solid #ddd; margin: 20px 0;">
              <p style="font-size: 14px; color: #666;">
                Ovo je automatska notifikacija iz vašeg SmartAlarm sistema.
                Provjerite status alarma putem aplikacije.
              </p>
            </div>
            <div style="background: #333; color: white; padding: 10px; text-align: center; font-size: 12px;">
              SmartAlarm System © 2026
            </div>
          </div>
        `
      };

      // Pošalji email
      await transporter.sendMail(mailOptions);
      
      // Spremi timestamp
      await admin.database()
        .ref('/devices/alarm_esp32_main/settings/lastEmailSent')
        .set(now);

      console.log(`Email sent successfully to ${recipientEmail}`);
      return null;
      
    } catch (error) {
      console.error('Error sending email:', error);
      return null;
    }
  });
