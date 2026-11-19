"AI-Enabled IoT Framework for Real-Time Driver Health Monitoring"

Project Description

This project presents an IoT-based driver health monitoring system that monitors health parameters and detects driver drowsiness to support safer driving.

The system uses an ESP32 microcontroller to collect sensor readings, monitor abnormal conditions, and send alerts. A separate camera-based module uses Python and OpenCV for drowsiness detection.

Hardware Components

ESP32 Development Board

MAX30102 Sensor

AD8232 ECG Sensor

GSR Sensor

NEO-6M GPS Module

I2C LCD Display

Buzzer

Camera

Software and Technologies

C++ (Arduino IDE)

Python

OpenCV

MediaPipe

ThingSpeak

Telegram Bot API

Major Features

Real-time heart rate and SpO2 monitoring

ECG signal monitoring

GSR monitoring

GPS location tracking

Cloud data visualization using ThingSpeak

Telegram health alerts

Camera-based drowsiness detection

Drowsiness alerts through Telegram

Project Files

Driver_health_monitoring_system.ino – ESP32 hardware programming.

driver_drowsiness.py – Python-based drowsiness detection.

Working

The ESP32 collects health-related sensor data and performs threshold-based analysis. The readings are displayed on the LCD and uploaded to ThingSpeak. When abnormal conditions are detected, alerts are sent through Telegram with available GPS location.

The separate camera-based module uses Python, OpenCV, and MediaPipe to monitor eye closure. When prolonged eye closure is detected, a drowsiness alert is sent through Telegram.

Conclusion

The proposed system combines IoT-based health monitoring, GPS tracking, cloud visualization, and camera-based drowsiness detection to support driver safety through timely warnings and alerts.

Note: This system is intended for monitoring and early warning purposes and is not a replacement for medical diagnosis.
