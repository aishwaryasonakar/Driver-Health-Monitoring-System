// ============================================================
// ESP32 DRIVER HEALTH MONITORING AND ALERTING SYSTEM
// ============================================================
//
// SENSORS:
// MAX30102  -> Heart Rate + SpO2
// AD8232    -> ECG
// GSR       -> Sweat / Skin Conductance
// NEO-6M    -> GPS Location
// LCD 16x2  -> Display
// BUZZER    -> Local Alert
// TELEGRAM  -> Remote Alert
//
// ESP32
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "MAX30105.h"
#include "spo2_algorithm.h"

#include <TinyGPSPlus.h>
#include <UniversalTelegramBot.h>


// ============================================================
// WIFI SETTINGS
// ============================================================

const char* WIFI_SSID     = "Realme10";
const char* WIFI_PASSWORD = "Aishu2005";

// ============================================================
// THINGSPEAK CLOUD SETTINGS
// ============================================================
// Replace this with your ThingSpeak CHANNEL WRITE API KEY
const char* THINGSPEAK_API_KEY = "TMZG4GS4B8OX6R63";

// ThingSpeak requires at least 15 seconds between updates.
// 20 seconds is used here so the existing sensor code is not disturbed.
const unsigned long THINGSPEAK_INTERVAL = 20000;
unsigned long lastThingSpeakUpload = 0;


// ============================================================
// TELEGRAM SETTINGS
// ============================================================

#define BOT_TOKEN "8825431037:AAHf5zqfv8WRz4xjfjWxpQW37GPLA20DuUQ"
#define CHAT_ID   "6299100891"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);


// ============================================================
// I2C PINS
// ============================================================

#define I2C_SDA 21
#define I2C_SCL 22


// ============================================================
// 16x2 I2C LCD
// ============================================================
//
// Most LCD modules use 0x27.
// If your LCD uses 0x3F, change 0x27 to 0x3F.
//

LiquidCrystal_I2C lcd(0x27, 16, 2);


// ============================================================
// MAX30102
// ============================================================

MAX30105 particleSensor;

#define MAX_BUFFER_SIZE 100

uint32_t irBuffer[MAX_BUFFER_SIZE];
uint32_t redBuffer[MAX_BUFFER_SIZE];

int bufferIndex = 0;

int32_t heartRate = 0;
int32_t spo2 = 0;

int8_t validHeartRate = 0;
int8_t validSPO2 = 0;

unsigned long lastSpO2Calculation = 0;

const unsigned long SPO2_CALC_INTERVAL = 2000;


// ============================================================
// AD8232 ECG
// ============================================================

#define ECG_PIN      36
#define ECG_LO_PLUS  32
#define ECG_LO_MINUS 33

int ecgValue = 0;


// ============================================================
// GSR SENSOR
// ============================================================

#define GSR_PIN 34

int gsrValue = 0;
int gsrBaseline = 0;

bool gsrBaselineReady = false;

// Adjust this value according to your GSR sensor
#define GSR_CHANGE_LIMIT 500


// ============================================================
// GPS NEO-6M
// ============================================================
//
// GPS TX -> ESP32 GPIO16
// GPS RX -> ESP32 GPIO17
//

#define GPS_RX 16
#define GPS_TX 17

HardwareSerial gpsSerial(2);

TinyGPSPlus gps;


// ============================================================
// BUZZER
// ============================================================

#define BUZZER_PIN 25


// ============================================================
// HEALTH ALERT LIMITS
// ============================================================

#define LOW_HR_LIMIT      50
#define HIGH_HR_LIMIT     120
#define LOW_SPO2_LIMIT    90


// ============================================================
// TIMING
// ============================================================

unsigned long lastLCDUpdate = 0;
unsigned long lastSerialUpdate = 0;
unsigned long lastWiFiCheck = 0;

const unsigned long LCD_INTERVAL = 200;
const unsigned long SERIAL_INTERVAL = 1000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;


// ============================================================
// TELEGRAM ALERT TIMING
// ============================================================

unsigned long lastTelegramAlert = 0;

const unsigned long TELEGRAM_COOLDOWN = 60000;


// ============================================================
// ALERT VARIABLES
// ============================================================

bool alertActive = false;

bool hrAlert = false;
bool spo2Alert = false;
bool gsrAlert = false;

String alertReason = "";


// ============================================================
// BUZZER VARIABLES
// ============================================================

bool buzzerActive = false;

unsigned long buzzerTimer = 0;

bool buzzerState = false;


// ============================================================
// LCD PAGE
// ============================================================
//
// Page 0:
// HR + SpO2
//
// Page 1:
// ECG + GSR
//
// Page 2:
// Status + GPS
//

int lcdPage = 0;

unsigned long lastLCDPageChange = 0;

const unsigned long LCD_PAGE_INTERVAL = 2000;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void readGPS();

void readECG();

void readGSR();

void readMAX30102();

void calculateHeartRateSpO2();

void checkConditions();

void updateLCD();

void updateSerial();

void handleBuzzer();

void sendTelegramAlert();

void checkWiFi();
void uploadToThingSpeak();

String getGPSCoordinates();

String getGPSLocation();

bool ecgLeadOff();


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" DRIVER HEALTH MONITORING SYSTEM");
  Serial.println(" ESP32");
  Serial.println("========================================");


  // ========================================================
  // GPIO SETUP
  // ========================================================

  pinMode(ECG_PIN, INPUT);

  pinMode(ECG_LO_PLUS, INPUT);

  pinMode(ECG_LO_MINUS, INPUT);

  pinMode(GSR_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);


  // ========================================================
  // I2C
  // ========================================================

  Wire.begin(I2C_SDA, I2C_SCL);


  // ========================================================
  // LCD
  // ========================================================

  lcd.init();

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("DRIVER HEALTH");

  lcd.setCursor(0, 1);
  lcd.print("MONITORING");

  delay(2000);


  // ========================================================
  // MAX30102 INITIALIZATION
  // ========================================================

  Serial.println("Checking MAX30102...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("ERROR: MAX30102 NOT FOUND!");

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("MAX30102 ERROR");

    lcd.setCursor(0, 1);
    lcd.print("CHECK WIRING");

    while (1)
    {
      delay(1000);
    }
  }

  Serial.println("MAX30102 FOUND");


  // MAX30102 configuration

  byte ledBrightness = 60;

  byte sampleAverage = 4;

  byte ledMode = 2;

  int sampleRate = 100;

  int pulseWidth = 411;

  int adcRange = 4096;

  particleSensor.setup(
    ledBrightness,
    sampleAverage,
    ledMode,
    sampleRate,
    pulseWidth,
    adcRange
  );

  particleSensor.setPulseAmplitudeRed(0x3F);

  particleSensor.setPulseAmplitudeIR(0x3F);

  particleSensor.setPulseAmplitudeGreen(0);


  // ========================================================
  // GPS INITIALIZATION
  // ========================================================

  Serial.println("Starting GPS...");

  gpsSerial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX,
    GPS_TX
  );

  Serial.println("GPS started");


  // ========================================================
  // WIFI
  // ========================================================

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long wifiStart = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - wifiStart < 15000
  )
  {
    delay(250);

    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi CONNECTED");

    Serial.print("IP Address: ");

    Serial.println(WiFi.localIP());

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("WIFI CONNECTED");

    lcd.setCursor(0, 1);
    lcd.print("SYSTEM READY");

    delay(1500);


    // ======================================================
    // TELEGRAM SSL
    // ======================================================

    secured_client.setInsecure();


    // ======================================================
    // TELEGRAM START MESSAGE
    // ======================================================

    bot.sendMessage(
      CHAT_ID,
      "Driver Health Monitoring System started.\nESP32 is online.",
      ""
    );
  }
  else
  {
    Serial.println("WiFi NOT CONNECTED");

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("WIFI NOT FOUND");

    lcd.setCursor(0, 1);
    lcd.print("SENSORS ACTIVE");

    delay(1500);
  }


  // ========================================================
  // INITIAL SENSOR VALUES
  // ========================================================

  readECG();

  readGSR();

  gsrBaseline = gsrValue;

  gsrBaselineReady = true;


  // ========================================================
  // INITIAL LCD
  // ========================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("HR: -- BPM");

  lcd.setCursor(0, 1);
  lcd.print("SpO2: -- %");


  Serial.println();
  Serial.println("SYSTEM READY");
  Serial.println("Place finger on MAX30102");
  Serial.println();

  Serial.println("ThingSpeak upload interval: 20 seconds");
  if (String(THINGSPEAK_API_KEY) == "YOUR_THINGSPEAK_WRITE_API_KEY")
  {
    Serial.println("WARNING: ThingSpeak Write API Key is NOT configured!");
  }
  else
  {
    Serial.println("ThingSpeak API Key configured.");
  }
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ========================================================
  // READ SENSORS CONTINUOUSLY
  // ========================================================

  readGPS();

  readECG();

  readGSR();

  readMAX30102();


  // Read GPS again
  readGPS();


  // ========================================================
  // CHECK HEALTH CONDITIONS
  // ========================================================

  checkConditions();


  // ========================================================
  // LCD UPDATE
  // ========================================================

  if (millis() - lastLCDUpdate >= LCD_INTERVAL)
  {
    lastLCDUpdate = millis();

    updateLCD();
  }


  // ========================================================
  // SERIAL MONITOR
  // ========================================================

  if (millis() - lastSerialUpdate >= SERIAL_INTERVAL)
  {
    lastSerialUpdate = millis();

    updateSerial();
  }


  // ========================================================
  // BUZZER
  // ========================================================

  handleBuzzer();


  // ========================================================
  // TELEGRAM ALERT
  // ========================================================

  if (alertActive)
  {
    sendTelegramAlert();
  }


  // ========================================================
  // WIFI CHECK
  // ========================================================

  if (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = millis();

    checkWiFi();
  }


  // ========================================================
  // THINGSPEAK CLOUD UPLOAD
  // ========================================================
  if (millis() - lastThingSpeakUpload >= THINGSPEAK_INTERVAL)
  {
    lastThingSpeakUpload = millis();

    Serial.println();
    Serial.println("Starting ThingSpeak upload...");
    uploadToThingSpeak();
  }


  delay(2);
}


// ============================================================
// READ GPS
// ============================================================

void readGPS()
{
  while (gpsSerial.available())
  {
    char c = gpsSerial.read();

    gps.encode(c);
  }
}


// ============================================================
// READ ECG
// ============================================================

void readECG()
{
  ecgValue = analogRead(ECG_PIN);
}


// ============================================================
// ECG LEAD OFF CHECK
// ============================================================

bool ecgLeadOff()
{
  if (
    digitalRead(ECG_LO_PLUS) == HIGH ||
    digitalRead(ECG_LO_MINUS) == HIGH
  )
  {
    return true;
  }

  return false;
}


// ============================================================
// READ GSR
// ============================================================

void readGSR()
{
  gsrValue = analogRead(GSR_PIN);
}


// ============================================================
// READ MAX30102
// ============================================================

void readMAX30102()
{
  particleSensor.check();


  while (particleSensor.available())
  {
    uint32_t redValue =
      particleSensor.getRed();

    uint32_t irValue =
      particleSensor.getIR();


    // Store samples

    redBuffer[bufferIndex] = redValue;

    irBuffer[bufferIndex] = irValue;

    bufferIndex++;


    // Get next sample

    particleSensor.nextSample();


    // ======================================================
    // BUFFER FULL
    // ======================================================

    if (bufferIndex >= MAX_BUFFER_SIZE)
    {
      calculateHeartRateSpO2();

      bufferIndex = 0;
    }
  }
}


// ============================================================
// CALCULATE HEART RATE + SpO2
// ============================================================

void calculateHeartRateSpO2()
{
  // Prevent calculation too frequently

  if (
    millis() - lastSpO2Calculation <
    SPO2_CALC_INTERVAL
  )
  {
    return;
  }

  lastSpO2Calculation = millis();


  // ========================================================
  // CHECK WHETHER FINGER IS PRESENT
  // ========================================================

  uint64_t irSum = 0;

  for (int i = 0; i < MAX_BUFFER_SIZE; i++)
  {
    irSum += irBuffer[i];
  }

  uint32_t irAverage =
    irSum / MAX_BUFFER_SIZE;


  Serial.print("IR Average: ");

  Serial.println(irAverage);


  // ========================================================
  // NO FINGER
  // ========================================================

  if (irAverage < 50000)
  {
    heartRate = 0;

    spo2 = 0;

    validHeartRate = 0;

    validSPO2 = 0;

    return;
  }


  // ========================================================
  // MAXIM SPO2 ALGORITHM
  // ========================================================

  int32_t calculatedHeartRate;

  int32_t calculatedSpO2;

  int8_t calculatedValidHeartRate;

  int8_t calculatedValidSpO2;


  maxim_heart_rate_and_oxygen_saturation(
    irBuffer,
    MAX_BUFFER_SIZE,
    redBuffer,
    &calculatedSpO2,
    &calculatedValidSpO2,
    &calculatedHeartRate,
    &calculatedValidHeartRate
  );


  // ========================================================
  // HEART RATE VALIDATION
  // ========================================================

  if (
    calculatedValidHeartRate == 1 &&
    calculatedHeartRate >= 30 &&
    calculatedHeartRate <= 220
  )
  {
    heartRate = calculatedHeartRate;

    validHeartRate = 1;
  }
  else
  {
    heartRate = 0;

    validHeartRate = 0;
  }


  // ========================================================
  // SpO2 VALIDATION
  // ========================================================

  if (
    calculatedValidSpO2 == 1 &&
    calculatedSpO2 >= 70 &&
    calculatedSpO2 <= 100
  )
  {
    spo2 = calculatedSpO2;

    validSPO2 = 1;
  }
  else
  {
    spo2 = 0;

    validSPO2 = 0;
  }


  Serial.print("Calculated HR: ");

  Serial.print(heartRate);

  Serial.print(" BPM | SpO2: ");

  Serial.print(spo2);

  Serial.println(" %");
}


// ============================================================
// CHECK HEALTH CONDITIONS
// ============================================================

void checkConditions()
{
  hrAlert = false;

  spo2Alert = false;

  gsrAlert = false;

  alertReason = "";


  // ========================================================
  // HEART RATE ALERT
  // ========================================================

  if (
    heartRate > 0 &&
    validHeartRate == 1
  )
  {
    if (heartRate < LOW_HR_LIMIT)
    {
      hrAlert = true;

      alertReason +=
        "Low Heart Rate";
    }

    else if (heartRate > HIGH_HR_LIMIT)
    {
      hrAlert = true;

      alertReason +=
        "High Heart Rate";
    }
  }


  // ========================================================
  // SpO2 ALERT
  // ========================================================

  if (
    spo2 > 0 &&
    validSPO2 == 1
  )
  {
    if (spo2 < LOW_SPO2_LIMIT)
    {
      spo2Alert = true;

      if (alertReason.length() > 0)
      {
        alertReason += " | ";
      }

      alertReason +=
        "Low SpO2";
    }
  }


  // ========================================================
  // GSR ALERT
  // ========================================================

  if (gsrBaselineReady)
  {
    int gsrDifference =
      abs(gsrValue - gsrBaseline);


    if (gsrDifference > GSR_CHANGE_LIMIT)
    {
      gsrAlert = true;

      if (alertReason.length() > 0)
      {
        alertReason += " | ";
      }

      alertReason +=
        "GSR Change";
    }
    else
    {
      // Slowly update baseline

      gsrBaseline =
        (gsrBaseline * 99 + gsrValue) / 100;
    }
  }


  // ========================================================
  // FINAL ALERT
  // ========================================================

  if (
    hrAlert ||
    spo2Alert ||
    gsrAlert
  )
  {
    alertActive = true;
  }
  else
  {
    alertActive = false;
  }
}


// ============================================================
// UPDATE 16x2 LCD
// ============================================================

void updateLCD()
{
  // ========================================================
  // CHANGE PAGE EVERY 2 SECONDS
  // ========================================================

  if (
    millis() - lastLCDPageChange >=
    LCD_PAGE_INTERVAL
  )
  {
    lastLCDPageChange = millis();

    lcdPage++;

    if (lcdPage > 2)
    {
      lcdPage = 0;
    }

    lcd.clear();
  }


  // ========================================================
  // PAGE 0
  // HEART RATE + SpO2
  // ========================================================

  if (lcdPage == 0)
  {
    lcd.setCursor(0, 0);

    lcd.print("HR:");

    if (heartRate > 0)
    {
      lcd.print(heartRate);
      lcd.print(" BPM");
    }
    else
    {
      lcd.print("-- BPM");
    }

    lcd.print("   ");


    lcd.setCursor(0, 1);

    lcd.print("SpO2:");

    if (spo2 > 0)
    {
      lcd.print(spo2);
      lcd.print(" %");
    }
    else
    {
      lcd.print("-- %");
    }

    lcd.print("    ");
  }


  // ========================================================
  // PAGE 1
  // ECG + GSR
  // ========================================================

  else if (lcdPage == 1)
  {
    lcd.setCursor(0, 0);

    lcd.print("ECG:");

    lcd.print(ecgValue);

    lcd.print("       ");


    lcd.setCursor(0, 1);

    lcd.print("GSR:");

    lcd.print(gsrValue);

    lcd.print("       ");
  }


  // ========================================================
  // PAGE 2
  // STATUS + GPS
  // ========================================================

  else if (lcdPage == 2)
  {
    lcd.setCursor(0, 0);


    if (alertActive)
    {
      lcd.print("!! ALERT !!");
    }

    else if (heartRate == 0)
    {
      lcd.print("PLACE FINGER");
    }

    else if (ecgLeadOff())
    {
      lcd.print("CHECK ECG");
    }

    else
    {
      lcd.print("STATUS: NORMAL");
    }


    lcd.print("    ");


    lcd.setCursor(0, 1);


    if (gps.location.isValid())
    {
      lcd.print("GPS: FIX");
    }

    else
    {
      lcd.print("GPS: NO FIX");
    }

    lcd.print("        ");
  }
}


// ============================================================
// SERIAL MONITOR
// ============================================================

void updateSerial()
{
  Serial.println();
  Serial.println("----------------------------------------");

  Serial.print("Heart Rate : ");

  if (heartRate > 0)
  {
    Serial.print(heartRate);

    Serial.println(" BPM");
  }
  else
  {
    Serial.println("NO READING");
  }


  Serial.print("SpO2       : ");

  if (spo2 > 0)
  {
    Serial.print(spo2);

    Serial.println(" %");
  }
  else
  {
    Serial.println("NO READING");
  }


  Serial.print("ECG        : ");

  Serial.println(ecgValue);


  Serial.print("ECG Leads  : ");

  if (ecgLeadOff())
  {
    Serial.println("LEAD OFF");
  }
  else
  {
    Serial.println("CONNECTED");
  }


  Serial.print("GSR        : ");

  Serial.println(gsrValue);


  Serial.print("GSR Base   : ");

  Serial.println(gsrBaseline);


  // ========================================================
  // GPS
  // ========================================================

  Serial.print("GPS        : ");

  if (gps.location.isValid())
  {
    Serial.println("FIX");

    Serial.print("Latitude   : ");

    Serial.println(
      gps.location.lat(),
      6
    );

    Serial.print("Longitude  : ");

    Serial.println(
      gps.location.lng(),
      6
    );
  }
  else
  {
    Serial.println("NO FIX");
  }


  // ========================================================
  // WIFI
  // ========================================================

  Serial.print("WiFi       : ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("CONNECTED");
  }
  else
  {
    Serial.println("DISCONNECTED");
  }


  // ========================================================
  // CONDITION
  // ========================================================

  Serial.print("Condition  : ");

  if (alertActive)
  {
    Serial.print("ALERT - ");

    Serial.println(alertReason);
  }
  else
  {
    Serial.println("NORMAL");
  }


  Serial.println("----------------------------------------");
}


// ============================================================
// BUZZER
// ============================================================

void handleBuzzer()
{
  if (!alertActive)
  {
    digitalWrite(BUZZER_PIN, LOW);

    buzzerState = false;

    return;
  }


  // ========================================================
  // BEEP ON/OFF
  // ========================================================

  if (buzzerState == false)
  {
    if (millis() - buzzerTimer >= 200)
    {
      buzzerTimer = millis();

      digitalWrite(BUZZER_PIN, HIGH);

      buzzerState = true;
    }
  }

  else
  {
    if (millis() - buzzerTimer >= 200)
    {
      buzzerTimer = millis();

      digitalWrite(BUZZER_PIN, LOW);

      buzzerState = false;
    }
  }
}


// ============================================================
// TELEGRAM ALERT
// ============================================================

void sendTelegramAlert()
{
  // Don't send repeatedly every loop

  if (
    millis() - lastTelegramAlert <
    TELEGRAM_COOLDOWN
  )
  {
    return;
  }


  // ========================================================
  // CHECK WIFI
  // ========================================================

  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }


  lastTelegramAlert = millis();


  // ========================================================
  // CREATE MESSAGE
  // ========================================================

  String message = "";

  message +=
    "🚨 DRIVER HEALTH ALERT 🚨\n\n";


  message +=
    "Reason: ";

  message +=
    alertReason;

  message +=
    "\n\n";


  // ========================================================
  // HEART RATE
  // ========================================================

  message +=
    "Heart Rate: ";

  if (heartRate > 0)
  {
    message += String(heartRate);

    message += " BPM";
  }
  else
  {
    message +=
      "No reading";
  }

  message += "\n";


  // ========================================================
  // SpO2
  // ========================================================

  message +=
    "SpO2: ";

  if (spo2 > 0)
  {
    message += String(spo2);

    message += " %";
  }
  else
  {
    message +=
      "No reading";
  }

  message += "\n";


  // ========================================================
  // ECG
  // ========================================================

  message +=
    "ECG ADC: ";

  message +=
    String(ecgValue);

  message += "\n";


  // ========================================================
  // GSR
  // ========================================================

  message +=
    "GSR: ";

  message +=
    String(gsrValue);

  message += "\n";


  // ========================================================
  // GPS
  // ========================================================

  if (gps.location.isValid())
  {
    double latitude =
      gps.location.lat();

    double longitude =
      gps.location.lng();


    message +=
      "\nGPS Location:\n";

    message +=
      "Latitude: ";

    message +=
      String(latitude, 6);

    message += "\n";


    message +=
      "Longitude: ";

    message +=
      String(longitude, 6);

    message += "\n";


    message +=
      "Google Maps:\n";

    message +=
      "https://maps.google.com/?q=";

    message +=
      String(latitude, 6);

    message += ",";

    message +=
      String(longitude, 6);
  }
  else
  {
    message +=
      "\nGPS: No location fix";
  }


  // ========================================================
  // SEND TELEGRAM
  // ========================================================

  Serial.println();

  Serial.println(
    "Sending Telegram alert..."
  );

  bool result =
    bot.sendMessage(
      CHAT_ID,
      message,
      ""
    );


  if (result)
  {
    Serial.println(
      "Telegram alert sent successfully."
    );
  }
  else
  {
    Serial.println(
      "Telegram alert failed."
    );
  }
}


// ============================================================
// WIFI CHECK
// ============================================================

void checkWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }


  Serial.println(
    "WiFi disconnected. Reconnecting..."
  );


  WiFi.disconnect();

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );
}


// ============================================================
// THINGSPEAK CLOUD UPLOAD
// ============================================================
// Field mapping:
// Field 1 = Heart Rate
// Field 2 = SpO2
// Field 3 = GSR
// Field 4 = Latitude
// Field 5 = Longitude
// Field 6 = ECG
//
// This function only uploads the already-read sensor variables.
// The existing sensor, LCD, buzzer and Telegram code is unchanged.
// ============================================================

void uploadToThingSpeak()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("ThingSpeak: WiFi not connected.");
    return;
  }

  if (String(THINGSPEAK_API_KEY) == "YOUR_THINGSPEAK_WRITE_API_KEY")
  {
    Serial.println("ThingSpeak: Please enter your Write API Key.");
    return;
  }

  double latitude = 0.0;
  double longitude = 0.0;

  if (gps.location.isValid())
  {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  String url = "https://api.thingspeak.com/update?api_key=";
  url += THINGSPEAK_API_KEY;

  url += "&field1=";
  url += String(heartRate);

  url += "&field2=";
  url += String(spo2);

  url += "&field3=";
  url += String(gsrValue);

  url += "&field4=";
  url += String(latitude, 6);

  url += "&field5=";
  url += String(longitude, 6);

  url += "&field6=";
  url += String(ecgValue);

  WiFiClientSecure thingSpeakClient;
  thingSpeakClient.setInsecure();

  HTTPClient http;

  if (!http.begin(thingSpeakClient, url))
  {
    Serial.println("ThingSpeak: HTTP begin failed.");
    return;
  }

  http.setTimeout(5000);

  int httpCode = http.GET();

  Serial.println();
  Serial.println("========== THINGSPEAK ==========");

  if (httpCode > 0)
  {
    String response = http.getString();

    Serial.print("HTTP Code : ");
    Serial.println(httpCode);

    Serial.print("ThingSpeak Response : ");
    Serial.println(response);

    if (httpCode == 200 && response.toInt() > 0)
    {
      Serial.println("ThingSpeak upload successful.");
      Serial.print("New ThingSpeak entry number: ");
      Serial.println(response);
    }
    else
    {
      Serial.println("ThingSpeak upload failed.");
      Serial.println("Response 0 usually means the API key/channel settings are incorrect or the update was rejected.");
    }
  }
  else
  {
    Serial.print("ThingSpeak HTTP Error : ");
    Serial.println(http.errorToString(httpCode));
  }

  Serial.println("================================");
  http.end();
}


// ============================================================
// GET GPS COORDINATES
// ============================================================

String getGPSCoordinates()
{
  if (gps.location.isValid())
  {
    String coordinates = "";

    coordinates +=
      String(
        gps.location.lat(),
        6
      );

    coordinates += ",";

    coordinates +=
      String(
        gps.location.lng(),
        6
      );

    return coordinates;
  }


  return "No GPS fix";
}


// ============================================================
// GET GOOGLE MAPS LOCATION
// ============================================================

String getGPSLocation()
{
  if (gps.location.isValid())
  {
    String location =
      "https://maps.google.com/?q=";

    location +=
      String(
        gps.location.lat(),
        6
      );

    location += ",";

    location +=
      String(
        gps.location.lng(),
        6
      );

    return location;
  }


  return "No GPS fix";
}