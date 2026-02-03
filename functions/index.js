const { onValueWritten } = require("firebase-functions/v2/database");
const admin = require("firebase-admin");
const nodemailer = require("nodemailer");

admin.initializeApp();

// Firebase Database Trigger - sluša promene na /devices/{deviceId}/alerts/last
exports.sendAlertEmail = onValueWritten(
  {
    ref: "/devices/{deviceId}/alerts/last",
    region: "europe-west1", // MORA biti isti region kao database!
  },
  async (event) => {
    // Ako je alert obrisan ili nije promenjen, ne radi ništa
    if (!event.data.after.exists()) {
      console.log("Alert deleted, skipping email");
      return null;
    }


    const alertMessage = event.data.after.val();
    const deviceId = event.params.deviceId;

    // Pročitaj status alarma
    const alarmStatusSnapshot = await admin
      .database()
      .ref(`/devices/${deviceId}/status`)
      .once("value");
    const alarmStatus = alarmStatusSnapshot.val();
    if (!alarmStatus || !alarmStatus.triggered) {
      console.log("Alarm nije trigerovan, email se ne šalje.");
      return null;
    }

    console.log(`Alert triggered for device ${deviceId}: ${alertMessage}`);

    // Dobij vrednosti environment varijabli
    const emailUserValue = process.env.EMAIL_USER;
    const emailPasswordValue = process.env.EMAIL_PASSWORD;

    if (!emailUserValue || !emailPasswordValue) {
      console.error("Email credentials not configured in environment variables");
      return null;
    }

    // Pročitaj notification email iz settings
    const settingsSnapshot = await admin
      .database()
      .ref(`/devices/${deviceId}/settings/notificationEmail`)
      .once("value");

    const recipientEmail = settingsSnapshot.val();

    if (!recipientEmail) {
      console.log("No notification email configured, skipping email send");
      return null;
    }

    // Rate limiting - provera da li je prošlo 5 minuta od zadnjeg emaila
    const lastEmailSnapshot = await admin
      .database()
      .ref(`/devices/${deviceId}/settings/lastEmailSent`)
      .once("value");

    const lastEmailTime = lastEmailSnapshot.val() || 0;
    const currentTime = Date.now();
    const fiveMinutes = 1 * 60 * 1000; // 1 minut u milisekundama

    if (currentTime - lastEmailTime < fiveMinutes) {
      console.log("Rate limit: Less than 5 minutes since last email, skipping");
      return null;
    }

    // Kreiraj transporter
    const transporter = nodemailer.createTransport({
      service: "gmail",
      auth: {
        user: emailUserValue,
        pass: emailPasswordValue,
      },
    });

    // Email konfiguracija
    const mailOptions = {
      from: `SmartAlarm <${emailUserValue}>`,
      to: recipientEmail,
      subject: "🚨 SmartAlarm Alert - Motion Detected!",
      html: `
        <div style="font-family: Arial, sans-serif; max-width: 600px; margin: 0 auto;">
          <h2 style="color: #d32f2f;">🚨 Motion Detected!</h2>
          <p><strong>Alert Message:</strong> ${alertMessage}</p>
          <p><strong>Device ID:</strong> ${deviceId}</p>
          <p><strong>Time:</strong> ${new Date().toLocaleString("en-GB", {
            timeZone: "Europe/Sarajevo",
          })}</p>
          <hr>
          <p style="color: #666; font-size: 12px;">
            This is an automated message from your SmartAlarm system.<br>
            You will not receive another alert for 5 minutes.
          </p>
        </div>
      `,
    };

    try {
      // Pošalji email
      await transporter.sendMail(mailOptions);
      console.log(`Email successfully sent to ${recipientEmail}`);

      // Ažuriraj timestamp zadnjeg poslanog emaila
      await admin
        .database()
        .ref(`/devices/${deviceId}/settings/lastEmailSent`)
        .set(currentTime);

      return { success: true, recipient: recipientEmail };
    } catch (error) {
      console.error("Error sending email:", error);
      return { success: false, error: error.message };
    }
  }
);
