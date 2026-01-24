import { Component, OnInit, OnDestroy } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { AlarmService, AlarmStatus, MotionLog, Alert } from '../../services/sensor-data.service';
import { Subscription } from 'rxjs';

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [CommonModule, FormsModule],
  templateUrl: './dashboard.component.html',
  styleUrl: './dashboard.component.scss'
})
export class DashboardComponent implements OnInit, OnDestroy {
  alarmStatus: AlarmStatus = { armed: false, triggered: false };
  pirState: number = 0;
  latestAlert: Alert | null = null;
  motionLogs: MotionLog[] = [];
  isConnected: boolean = false;
  pinCode: string = '';  // PIN input
  pinError: string = '';  // Poruka o grešci
  
  // Schedule podešavanja
  scheduleEnabled: boolean = false;
  scheduleStartHour: number = 22;
  scheduleStartMinute: number = 0;
  scheduleEndHour: number = 6;
  scheduleEndMinute: number = 0;
  
  // Email notifikacije
  notificationEmail: string = '';
  
  // Tabs
  activeTab: string = 'dashboard';
  settingsSubView: string = 'email'; // 'email' or 'schedule'
  
  private statusSubscription?: Subscription;
  private pirSubscription?: Subscription;
  private alertSubscription?: Subscription;
  private logsSubscription?: Subscription;
  private scheduleSubscription?: Subscription;

  constructor(private alarmService: AlarmService) {}

  ngOnInit() {
    // Proveri konekciju
    this.checkConnection();
    
    // Inicijalizuj default schedule vrijednosti u Firebase ako ne postoje
    this.initializeSchedule();
    
    // Slušaj status alarma
    this.statusSubscription = this.alarmService.getAlarmStatus().subscribe({
      next: (status) => {
        this.alarmStatus = status;
        this.isConnected = true;
      },
      error: (err) => {
        console.error('Error reading alarm status:', err);
        this.isConnected = false;
      }
    });
    
    // Slušaj PIR senzor
    this.pirSubscription = this.alarmService.getPIRStatus().subscribe({
      next: (state) => {
        this.pirState = state;
      },
      error: (err) => {
        console.error('Error reading PIR status:', err);
      }
    });
    
    // Slušaj alerte
    this.alertSubscription = this.alarmService.getLatestAlert().subscribe({
      next: (alert) => {
        this.latestAlert = alert;
      },
      error: (err) => {
        console.error('Error reading alerts:', err);
      }
    });
    
    // Slušaj motion logs
    this.logsSubscription = this.alarmService.getMotionLogs(10).subscribe({
      next: (logs) => {
        this.motionLogs = logs;
      },
      error: (err) => {
        console.error('Error reading motion logs:', err);
      }
    });
    
    // Slušaj schedule podešavanja
    this.scheduleSubscription = this.alarmService.getScheduleSettings().subscribe({
      next: (schedule) => {
        this.scheduleEnabled = schedule.enabled;
        this.scheduleStartHour = schedule.startHour;
        this.scheduleStartMinute = schedule.startMinute;
        this.scheduleEndHour = schedule.endHour;
        this.scheduleEndMinute = schedule.endMinute;
      },
      error: (err) => {
        console.error('Error reading schedule settings:', err);
      }
    });
  }

  ngOnDestroy() {
    // Cleanup subscriptions
    this.statusSubscription?.unsubscribe();
    this.pirSubscription?.unsubscribe();
    this.alertSubscription?.unsubscribe();
    this.logsSubscription?.unsubscribe();
    this.scheduleSubscription?.unsubscribe();
  }

  async checkConnection() {
    this.isConnected = await this.alarmService.isDeviceConnected();
  }
  
  async initializeSchedule() {
    try {
      // Postavi default schedule vrijednosti ako ne postoje
      await this.alarmService.updateSchedule(false, 22, 0, 6, 0);
      console.log('✅ Schedule initialized with defaults');
    } catch (error) {
      console.error('❌ Error initializing schedule:', error);
    }
  }

  async armAlarm() {
    if (!this.pinCode || this.pinCode.length === 0) {
      this.pinError = 'Unesite PIN kod!';
      return;
    }
    
    try {
      await this.alarmService.armAlarmWithPin(this.pinCode);
      this.pinError = '';
      this.pinCode = ''; // Reset PIN nakon uspješnog unosa
    } catch (error) {
      console.error('Error arming alarm:', error);
      this.pinError = 'Greška pri slanju komande!';
    }
  }

  async disarmAlarm() {
    if (!this.pinCode || this.pinCode.length === 0) {
      this.pinError = 'Unesite PIN kod!';
      return;
    }
    
    try {
      await this.alarmService.disarmAlarmWithPin(this.pinCode);
      this.pinError = '';
      this.pinCode = ''; // Reset PIN nakon uspješnog unosa
    } catch (error) {
      console.error('Error disarming alarm:', error);
      this.pinError = 'Greška pri slanju komande!';
    }
  }
  
  async updateSchedule() {
    try {
      console.log('🔧 Updating schedule:', {
        enabled: this.scheduleEnabled,
        start: `${this.scheduleStartHour}:${this.scheduleStartMinute}`,
        end: `${this.scheduleEndHour}:${this.scheduleEndMinute}`
      });
      
      await this.alarmService.updateSchedule(
        this.scheduleEnabled,
        this.scheduleStartHour,
        this.scheduleStartMinute,
        this.scheduleEndHour,
        this.scheduleEndMinute
      );
      
      console.log('✅ Schedule updated successfully in Firebase');
    } catch (error) {
      console.error('❌ Error updating schedule:', error);
    }
  }

  async updateNotificationEmail() {
    try {
      await this.alarmService.updateNotificationEmail(this.notificationEmail);
      console.log('✅ Email updated:', this.notificationEmail);
      alert('Email adresa sačuvana!');
    } catch (error) {
      console.error('❌ Error updating email:', error);
      alert('Greška pri čuvanju email-a');
    }
  }

  getAlarmStatusText(): string {
    if (!this.isConnected) return 'DISCONNECTED';
    if (this.alarmStatus.triggered) return 'TRIGGERED!';
    if (this.alarmStatus.armed) return 'ARMED';
    return 'DISARMED';
  }

  getAlarmStatusColor(): string {
    if (!this.isConnected) return '#757575'; // gray
    if (this.alarmStatus.triggered) return '#f44336'; // red
    if (this.alarmStatus.armed) return '#ff9800'; // orange
    return '#4caf50'; // green
  }

  formatTimestamp(timestamp: number): string {
    const date = new Date(timestamp);
    return date.toLocaleString('sr-RS', {
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  }
  
  switchTab(tab: string) {
    this.activeTab = tab;
    console.log('Switched to tab:', tab);
  }

  switchSettingsView(view: string) {
    this.settingsSubView = view;
    console.log('Switched settings view to:', view);
  }
}
