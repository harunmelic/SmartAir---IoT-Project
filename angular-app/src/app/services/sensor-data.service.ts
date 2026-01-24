import { Injectable } from '@angular/core';
import { Database, ref, onValue, set, get, query, orderByKey, limitToLast } from '@angular/fire/database';
import { Observable } from 'rxjs';

export interface AlarmStatus {
  armed: boolean;
  triggered: boolean;
  pirState?: number;
  timestamp?: number;
}

export interface MotionLog {
  timestamp: number;
  message: string;
}

export interface Alert {
  last: string;
  timestamp: number;
}

export interface Schedule {
  enabled: boolean;
  startHour: number;
  startMinute: number;
  endHour: number;
  endMinute: number;
}

@Injectable({
  providedIn: 'root'
})
export class AlarmService {
  private deviceId = 'alarm_esp32_main';

  constructor(private db: Database) {}

  // Čita status alarma (armed/triggered)
  getAlarmStatus(): Observable<AlarmStatus> {
    return new Observable(observer => {
      const statusRef = ref(this.db, `devices/${this.deviceId}/status`);
      
      onValue(statusRef, (snapshot) => {
        const data = snapshot.val();
        if (data) {
          observer.next({
            armed: data.armed || false,
            triggered: data.triggered || false,
            timestamp: Date.now()
          });
        }
      }, (error) => {
        observer.error(error);
      });
    });
  }

  // Čita PIR senzor status
  getPIRStatus(): Observable<number> {
    return new Observable(observer => {
      const pirRef = ref(this.db, `devices/${this.deviceId}/sensor/pir`);
      
      onValue(pirRef, (snapshot) => {
        observer.next(snapshot.val() || 0);
      }, (error) => {
        observer.error(error);
      });
    });
  }

  // ARM alarm (aktivacija)
  async armAlarm(): Promise<void> {
    const statusRef = ref(this.db, `devices/${this.deviceId}/status/armed`);
    return set(statusRef, true);
  }

  // DISARM alarm (deaktivacija)
  async disarmAlarm(): Promise<void> {
    const statusRef = ref(this.db, `devices/${this.deviceId}/status/armed`);
    return set(statusRef, false);
  }

  // ARM alarm sa PIN kodom
  async armAlarmWithPin(pin: string): Promise<void> {
    const commandsRef = ref(this.db, `devices/${this.deviceId}/commands`);
    return set(commandsRef, {
      pin: pin,
      action: true,
      timestamp: Date.now()
    });
  }

  // DISARM alarm sa PIN kodom
  async disarmAlarmWithPin(pin: string): Promise<void> {
    const commandsRef = ref(this.db, `devices/${this.deviceId}/commands`);
    return set(commandsRef, {
      pin: pin,
      action: false,
      timestamp: Date.now()
    });
  }

  // Čita poslednje alerte
  getLatestAlert(): Observable<Alert | null> {
    return new Observable(observer => {
      const alertRef = ref(this.db, `devices/${this.deviceId}/alerts`);
      
      onValue(alertRef, (snapshot) => {
        const data = snapshot.val();
        if (data && data.last) {
          observer.next({
            last: data.last,
            timestamp: data.timestamp || Date.now()
          });
        } else {
          observer.next(null);
        }
      }, (error) => {
        observer.error(error);
      });
    });
  }

  // Čita motion logs (poslednji logovi pokreta)
  getMotionLogs(limit: number = 10): Observable<MotionLog[]> {
    return new Observable(observer => {
      const logsRef = query(
        ref(this.db, `devices/${this.deviceId}/motionLogs`),
        orderByKey(),
        limitToLast(limit)
      );
      
      onValue(logsRef, (snapshot) => {
        const logs: MotionLog[] = [];
        snapshot.forEach((childSnapshot) => {
          const timestampSeconds = parseInt(childSnapshot.key || '0');
          logs.push({
            timestamp: timestampSeconds * 1000, // Konvertuj u milisekunde
            message: childSnapshot.val()
          });
        });
        observer.next(logs.reverse());
      }, (error) => {
        observer.error(error);
      });
    });
  }

  // Proverava da li je ESP32 povezan
  async isDeviceConnected(): Promise<boolean> {
    const statusRef = ref(this.db, `devices/${this.deviceId}/status`);
    const snapshot = await get(statusRef);
    return snapshot.exists();
  }
  
  // Čita schedule podešavanja
  getScheduleSettings(): Observable<Schedule> {
    return new Observable(observer => {
      const scheduleRef = ref(this.db, `devices/${this.deviceId}/schedule`);
      
      onValue(scheduleRef, (snapshot) => {
        const data = snapshot.val();
        observer.next({
          enabled: data?.enabled || false,
          startHour: data?.startHour || 22,
          startMinute: data?.startMinute || 0,
          endHour: data?.endHour || 6,
          endMinute: data?.endMinute || 0
        });
      }, (error) => {
        observer.error(error);
      });
    });
  }
  
  // Ažurira schedule podešavanja
  async updateSchedule(enabled: boolean, startHour: number, startMinute: number, endHour: number, endMinute: number): Promise<void> {
    const scheduleRef = ref(this.db, `devices/${this.deviceId}/schedule`);
    
    console.log('📡 Sending to Firebase:', {
      path: `devices/${this.deviceId}/schedule`,
      data: { enabled, startHour, startMinute, endHour, endMinute }
    });
    
    return set(scheduleRef, {
      enabled: enabled,
      startHour: startHour,
      startMinute: startMinute,
      endHour: endHour,
      endMinute: endMinute
    });
  }

  // Ažurira email za notifikacije
  async updateNotificationEmail(email: string): Promise<void> {
    const emailRef = ref(this.db, `devices/${this.deviceId}/settings/notificationEmail`);
    return set(emailRef, email);
  }

  // Dohvata email za notifikacije
  getNotificationEmail() {
    const emailRef = ref(this.db, `devices/${this.deviceId}/settings/notificationEmail`);
    return onValue(emailRef, () => {});
  }
}