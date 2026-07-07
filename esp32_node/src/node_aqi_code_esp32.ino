// ============================================================================
// Cloud Connected AQI Node A – ESP32 + TFT OLED + SIM800L + Blynk
// ============================================================================
// This sketch measures PM1, PM2.5, CO and SO2, computes a time‑weighted AQI,
// displays values on a TFT OLED, sends SMS alerts via SIM800L and publishes
// data to Blynk Cloud.
//

// ============================================================================

// --------------------------- Blynk configuration ----------------------------

// Replace these with your own values locally, but keep placeholders in GitHub.
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxx"        // <YOUR_BLYNK_TEMPLATE_ID>
#define BLYNK_DEVICE_NAME   "Esp_NodeA"
#define BLYNK_TEMPLATE_NAME "Air Quality Monitor"

// Blynk auth token – keep a placeholder in the public repo
#define BLYNK_AUTH_TOKEN    "<YOUR_BLYNK_AUTH_TOKEN>"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Declare forward so we can use it in setup()
void temp();

// Wi‑Fi credentials – use placeholders in GitHub
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "<YOUR_WIFI_SSID>";      // e.g. "MyHomeWiFi"
char pass[] = "<YOUR_WIFI_PASSWORD>";  // e.g. "StrongPassword123"

BlynkTimer timer;

// --------------------------- Cloud data variables ---------------------------
// Values sent to Blynk virtual pins
float pm1_a;   // V5
float pm25_a;  // V6
float co_a;    // V7
float so_a;    // V8
float aqi_a;   // V9

// --------------------------- Sleep mode configuration -----------------------

#define uS_TO_S_FACTOR 1000000  // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP  1        // Deep sleep duration (seconds)

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int runcount  = 0;

// --------------------------- Sensor pin definitions -------------------------

#define PM1PIN  26
#define PM25PIN 33
#define CPIN    35
#define SPIN    34

// Timing and DSM501A pulse variables
unsigned long duration1;
unsigned long duration2;
unsigned long starttime;
unsigned long sampletime_ms      = 3000;  // sample window (ms)
unsigned long lowpulseoccupancy1 = 0;
unsigned long lowpulseoccupancy2 = 0;

// Pollutant concentrations and AQI components
float ratio1  = 0;
float ratio2  = 0;
float concentration1 = 0;  // PM1
float concentration2 = 0;  // PM2.5
float tempSO         = 0;  // SO2 ppm
float tempCO         = 0;  // CO ppm

// Composite weights for AQI calculation
float composite_weightage_PM25 = 0.50;
float composite_weightage_PM1  = 0.25;
float composite_weightage_CO   = 0.15;
float composite_weightage_SO   = 0.10;

float rc         = 0;
float AQI25      = 0;
float AQI1       = 0;
float AQICO      = 0;
float AQISO      = 0;
float hourly_aqi = 0;
float pm2        = 0;
float pm1        = 0;

// --------------------------- Function declarations --------------------------

float calculateCOPPM();
float calculateSO2PPM();
void  PM1andPM2calc();
float calculatePM25_AQI(float arr[], int maxsize, int Front, int Rear);
float calculatePM1_AQI(float arr[], int maxsize, int Front, int Rear);
float calculateCO_AQI(float arr[], int maxsize, int Front, int Rear);
float calculateSO_AQI(float arr[], int maxsize, int Front, int Rear);
float weighted_average(float values[], int maxsize, float composite_weightage,
                       int Front, int Rear);
void  OLED();
void  printAirQualityDescription(float aqi);

// ============================================================================
// TFT OLED display (ST7735)
// ============================================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT pins for ESP32
#define TFT_CS   5
#define TFT_RST  15
#define TFT_DC   32
#define TFT_MOSI 23
#define TFT_SCLK 18

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ============================================================================
// SIM800L GSM module
// ============================================================================

// Phone number – keep placeholder in GitHub
const String PHONE = "<YOUR_PHONE_NUMBER>";  // e.g. "+9233XXXXXXXX"

HardwareSerial sim800(1);
#define RX_PIN    4
#define TX_PIN    2
#define BAUD_RATE 9600

// ============================================================================
// Circular queue / RTC storage for 24‑hour rolling data
// ============================================================================

const int maxSize = 24;

RTC_DATA_ATTR float pm1Arr[maxSize];
RTC_DATA_ATTR int   pm1Front = -1;
RTC_DATA_ATTR int   pm1Rear  = -1;

RTC_DATA_ATTR float pm25Arr[maxSize];
RTC_DATA_ATTR int   pm25Front = -1;
RTC_DATA_ATTR int   pm25Rear  = -1;

RTC_DATA_ATTR float coArr[maxSize];
RTC_DATA_ATTR int   coFront = -1;
RTC_DATA_ATTR int   coRear  = -1;

RTC_DATA_ATTR float soArr[maxSize];
RTC_DATA_ATTR int   soFront = -1;
RTC_DATA_ATTR int   soRear  = -1;

// Queue helpers
bool Queue_is_Empty(int front, int rear) {
  return (front == -1 && rear == -1);
}

bool Queue_is_Full(int front, int rear) {
  return ((rear + 1) % maxSize == front);
}

void Queue_enqueue(float value, float arr[], int &front, int &rear) {
  if (Queue_is_Empty(front, rear)) {
    front = rear = 0;
    arr[rear] = value;
  } else if (!Queue_is_Full(front, rear)) {
    rear = (rear + 1) % maxSize;
    arr[rear] = value;
  } else {
    // Queue is full – overwrite oldest value
    front = (front + 1) % maxSize;
    rear  = (rear + 1) % maxSize;
    arr[rear] = value;
  }
}

void Queue_display(float arr[], int front, int rear) {
  if (Queue_is_Empty(front, rear)) {
    Serial.println("Queue is empty!");
    return;
  }

  int i = front;
  do {
    Serial.print(arr[i]);
    Serial.print(" ");
    i = (i + 1) % maxSize;
  } while (i != (rear + 1) % maxSize);

  Serial.println();
}




// ============================================================================
// SIM800L – send SMS with pollutant readings and AQI
// ============================================================================

void sendSMS(float pm1, float pm2, float co, float so2, float aqi) {
  sim800.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(1000);

  sim800.println("AT");
  delay(1000);
  sim800.println("AT+CMGF=1");                 // Text mode
  delay(1000);
  sim800.println("AT+CMGS=\"" + PHONE + "\""); // Recipient
  delay(1000);

  String message = "PM1: " + String(pm1) + " ug/m3\n";
  message       += "PM2.5: " + String(pm2) + " ug/m3\n";
  message       += "CO: " + String(co) + " ppm\n";
  message       += "SO2: " + String(so2) + " ppm\n";
  message       += "AQI: " + String(aqi);

  sim800.println(message);
  delay(1000);
  sim800.write(26);  // CTRL+Z
  delay(1000);
}

// ============================================================================
// Blynk – send data to virtual pins
// ============================================================================

void sendSensor() {
  Blynk.virtualWrite(V5, pm1_a);
  Blynk.virtualWrite(V6, pm25_a);
  Blynk.virtualWrite(V7, co_a);
  delay(2000);
  Blynk.virtualWrite(V8, so_a);
  Blynk.virtualWrite(V9, aqi_a);

  Serial.println("Sending data to Blynk Cloud");
}



// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);

  // Connect to Blynk (and Wi‑Fi)
  Blynk.begin(auth, ssid, pass);

  // Initialise TFT display
  tft.initR(INITR_BLACKTAB);
  Serial.println("Initialised TFT display");
  tft.fillScreen(ST77XX_BLACK);

  int w = tft.width();
  int h = tft.height();
  Serial.print("width = ");
  Serial.println(w);
  Serial.print("height = ");
  Serial.println(h);

  // ---------------------- First boot vs subsequent runs ---------------------

  if (bootCount == 0) {
    bootCount++;
    Serial.print("boot count number : ");
    Serial.println(bootCount);

    runcount++;

    PM1andPM2calc();
    pm1    = concentration1;
    pm2    = concentration2;
    tempCO = calculateCOPPM();
    tempSO = calculateSO2PPM();

    // Store values in circular queues
    Queue_enqueue(concentration1, pm1Arr, pm1Front, pm1Rear);
    Serial.print("Enqueued PM1: ");
    Serial.println(concentration1);
    Queue_display(pm1Arr, pm1Front, pm1Rear);

    Queue_enqueue(concentration2, pm25Arr, pm25Front, pm25Rear);
    Serial.print("Enqueued PM2.5: ");
    Serial.println(concentration2);
    Queue_display(pm25Arr, pm25Front, pm25Rear);

    Queue_enqueue(tempCO, coArr, coFront, coRear);
    Serial.print("Enqueued CO: ");
    Serial.println(tempCO, 4);
    Queue_display(coArr, coFront, coRear);

    Queue_enqueue(tempSO, soArr, soFront, soRear);
    Serial.print("Enqueued SO2: ");
    Serial.println(tempSO, 4);
    Queue_display(soArr, soFront, soRear);

    Serial.println("----------------------");

  } else {
    // Subsequent wake‑ups
    PM1andPM2calc();
    pm1    = concentration1;
    pm2    = concentration2;
    tempCO = calculateCOPPM();
    tempSO = calculateSO2PPM();

    Queue_enqueue(concentration1, pm1Arr, pm1Front, pm1Rear);
    Serial.print("Enqueued PM1: ");
    Serial.println(concentration1);
    Queue_display(pm1Arr, pm1Front, pm1Rear);

    Queue_enqueue(concentration2, pm25Arr, pm25Front, pm25Rear);
    Serial.print("Enqueued PM2.5: ");
    Serial.println(concentration2);
    Queue_display(pm25Arr, pm25Front, pm25Rear);

    Queue_enqueue(tempCO, coArr, coFront, coRear);
    Serial.print("Enqueued CO: ");
    Serial.println(tempCO, 4);
    Queue_display(coArr, coFront, coRear);

    Queue_enqueue(tempSO, soArr, soFront, soRear);
    Serial.print("Enqueued SO2: ");
    Serial.println(tempSO, 4);
    Queue_display(soArr, soFront, soRear);

    Serial.println("----------------------");

    runcount++;
    Serial.print("runcount number : ");
    Serial.println(runcount);
  }

  delay(3000);

  // ---------------------- AQI calculation after 2+ runs --------------------

  if (runcount >= 2) {
    AQI25 = calculatePM25_AQI(pm25Arr, maxSize, pm25Front, pm25Rear);
    Serial.print("PM2.5 AQI: ");
    Serial.println(AQI25, 4);

    AQI1 = calculatePM1_AQI(pm1Arr, maxSize, pm1Front, pm1Rear);
    Serial.print("PM1 AQI: ");
    Serial.println(AQI1, 4);

    AQISO = calculateSO_AQI(soArr, maxSize, soFront, soRear);
    Serial.print("SO2 AQI: ");
    Serial.println(AQISO, 4);

    AQICO = calculateCO_AQI(coArr, maxSize, coFront, coRear);
    Serial.print("CO AQI: ");
    Serial.println(AQICO, 4);

    hourly_aqi = (AQI25 * 0.35) + (AQI1 * 0.25) + (AQISO * 0.20) + (AQICO * 0.20);

    Serial.print("hourly AQI is : ");
    Serial.println(hourly_aqi);

    printAirQualityDescription(hourly_aqi);
  }

  rc = runcount;

  // ---------------------- Update OLED / TFT display ------------------------

  OLED();

  // ---------------------- SMS and Blynk publishing -------------------------

  // Send SMS with readings and AQI
  sendSMS(concentration1, concentration2, tempCO, tempSO, hourly_aqi);

  // Prepare values for Blynk
  pm1_a  = concentration1;
  pm25_a = concentration2;
  co_a   = tempCO;
  so_a   = tempSO;
  aqi_a  = hourly_aqi;

  // Send data to Blynk Cloud
  Blynk.begin(auth, ssid, pass);
  for (int i = 0; i < 2; i++) {
    delay(3000);
    timer.setInterval(1000L, sendSensor);
    delay(3000);
    temp();
  }

  tft.fillScreen(ST77XX_BLACK);

  // ---------------------- Enter deep sleep -------------------------------

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void temp() {
  Blynk.run();
  timer.run();
}
