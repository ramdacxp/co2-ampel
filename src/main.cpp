#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "MHZ19.h"
#include <SoftwareSerial.h>
#include <Bounce2.h>
#include "uptime.h"

#define CO2LEVEL_MAX_OK 800
#define CO2LEVEL_MAX_WARN 1000
#define SENSOR_INIT_TIME 60000
#define SENSOR_UPDATE_INTERVAL 10000
#define SENSOR_CALIBRATION_DURATION 180000

/*******************************************************************************
 * D1 Mini C defines use the GPIO values, not the Dxx labels printed on the board.
 * GPIO 2 (D4) is connected to the built in LED; also available as LED_BUILTIN.
 ******************************************************************************/
#define D1MINI_D0 16
// #define D1MINI_D1 5 // used by I2C / LCD display
// #define D1MINI_D2 4 // used by I2C / LCD display
#define D1MINI_D3 0
// #define D1MINI_D4 2 // internal LED, do not use
#define D1MINI_D5 14
#define D1MINI_D6 12
#define D1MINI_D7 13
#define D1MINI_D8 15

// Taster
#define BTN1 D1MINI_D3
#define BTN2 D1MINI_D8
#define BTN_DEBOUNCETIME 50

// LCD I2C Pins
// - GND -> G
// - VCC -> 5V
// - SDA -> D2
// - SCL -> D1

// CO2-Pins (right to left)
// RX/TX must be cross-connected
// - 3 (black, GND) -> G
// - 4 (red, VIN) -> 5V
// - 5 (blue, RXD) -> D6 (SoftSerial TX)
// - 6 (green, TXD) -> D5 (SoftSerial RX)
#define SENSOR_RX D1MINI_D5
#define SENSOR_TX D1MINI_D6

MHZ19 sensor;
SoftwareSerial softSerial(SENSOR_RX, SENSOR_TX);

LiquidCrystal_I2C lcd(0x27, 16, 2);

Bounce bouncer1 = Bounce();
Bounce bouncer2 = Bounce();

byte characterUpperTwo[] = {
    B01100,
    B10010,
    B00100,
    B01000,
    B11110,
    B00000,
    B00000,
    B00000};

byte characterWlan[] = {
    B00000,
    B11111,
    B00001,
    B11101,
    B00101,
    B10101,
    B00000,
    B00000};

byte characterBtnL[] = {
    B01000,
    B01000,
    B01000,
    B01000,
    B01110,
    B00000,
    B01110,
    B11111};

byte characterBtnR[] = {
    B01100,
    B01010,
    B01100,
    B01010,
    B01010,
    B00000,
    B01110,
    B11111};

unsigned long tsNow = 0;
unsigned long tslastUpdate = 0;
unsigned long tsCalStart = 0;
int sensorCo2 = -1;
float sensorTemp = -1;

enum MainAction
{
  Measure,
  Calibrate,
  Info
};

MainAction action = MainAction::Measure;

const char *getCo2Message()
{
  if (tsNow < SENSOR_INIT_TIME)
    return "Warten...";
  else if (sensorCo2 <= CO2LEVEL_MAX_OK)
    return "OK";
  else if (sensorCo2 <= CO2LEVEL_MAX_WARN)
    return "Warnung";
  else
    return "Kritisch";
}

void printSensorData()
{
  // Temperature
  lcd.setCursor(0, 0);
  lcd.print("CO\x02-Ampel ");
  lcd.print(String(sensorTemp, 1));
  lcd.print("\xDF"); // °
  lcd.print("C");

  // Message
  lcd.setCursor(0, 1);
  lcd.print(getCo2Message());
  lcd.print("        ");

  // CO2
  lcd.setCursor(10, 1);
  lcd.print(String(sensorCo2));
  lcd.print("     ");
}

void updateSensorData()
{
  // skip update if interval not reached
  if (tsNow - tslastUpdate < SENSOR_UPDATE_INTERVAL)
    return;

  // update data
  tslastUpdate = tsNow;

  Serial.println("Reading sensor ...");

  // read data
  // lcd.noBacklight();
  sensorCo2 = sensor.getCO2();
  sensorTemp = sensor.getTemperature(true);

  Serial.print("- CO² (ppm): ");
  Serial.println(sensorCo2);
  Serial.print("- Temperature (°C): ");
  Serial.println(sensorTemp);
  Serial.print("- ErrorCode: ");
  Serial.println(sensor.errorCode);

  if (action == MainAction::Measure)
    printSensorData();
}

void changeAction()
{
  if (action == MainAction::Measure)
  {
    action = MainAction::Calibrate;
    lcd.setCursor(0, 0);
    lcd.print("Kalibrierung    ");
    lcd.setCursor(0, 1);
    lcd.print("\x04-Men\xF5   \x05-Start");
  }
  else if (action == MainAction::Calibrate)
  {
    action = MainAction::Info;
    lcd.setCursor(0, 0);
    lcd.print("Infoanzeige     ");
    lcd.setCursor(0, 1);
    lcd.print("\x04-Men\xF5    \x05-Wert");
  }
  else
  {
    action = MainAction::Measure;
    lcd.setCursor(0, 0);
    lcd.print("Messmodus       ");
    lcd.setCursor(0, 1);
    lcd.print("\x04-Men\xF5          ");
  }
}

void startCalibration()
{
  lcd.setCursor(0, 1);
  lcd.print("Kalibr. startet ");

  // sensor.setRange(5000);
  // delay(500);
  sensor.autoCalibration(false);
  delay(500);
  sensor.calibrate();
  delay(500);

  tsCalStart = tsNow;
}

void printInfo()
{
  static uint infoId;
  infoId++;
  if (infoId > 4)
    infoId = 0;

  lcd.clear();
  if (infoId == 0)
  {
    lcd.setCursor(0, 0);
    lcd.print("Sensor Status");
    lcd.setCursor(0, 1);
    lcd.print("ErrorCode: ");
    lcd.print(String(sensor.errorCode));
  }
  else if (infoId == 1)
  {
    uptime::calculateUptime();

    unsigned long d = uptime::getDays();
    unsigned long h = uptime::getHours();
    unsigned long m = uptime::getMinutes();
    unsigned long s = uptime::getSeconds();

    lcd.setCursor(0, 0);
    lcd.print("Ampel aktiv seit");
    lcd.setCursor(0, 1);
    lcd.print(String(d));
    lcd.print("d ");
    lcd.print(String(h));
    lcd.print(":");
    if (m < 10)
      lcd.print("0");
    lcd.print(String(m));
    lcd.print(":");
    if (s < 10)
      lcd.print("0");
    lcd.print(String(s));
  }
  else if (infoId == 2)
  {
    byte accuracy = sensor.getAccuracy();
    lcd.setCursor(0, 0);
    lcd.print("Genauigkeit");
    lcd.setCursor(0, 1);
    lcd.print(String(accuracy));
  }
  else if (infoId == 3)
  {
    int bgco2 = sensor.getBackgroundCO2();
    lcd.setCursor(0, 0);
    lcd.print("Hintergrund CO\x02");
    lcd.setCursor(0, 1);
    lcd.print(String(bgco2));
  }
  else if (infoId == 4)
  {
    char version[4];
    sensor.getVersion(version);

    lcd.setCursor(0, 0);
    lcd.print("Sensor Firmware ");
    lcd.setCursor(0, 1);
    lcd.print("Version: ");
    lcd.print(version[0]);
    lcd.print(version[1]);
    lcd.print(".");
    lcd.print(version[2]);
    lcd.print(version[3]);
  }
}

void updateCalibrationProgress()
{
  int remainingSek = ((tsCalStart + SENSOR_CALIBRATION_DURATION) - tsNow) / 1000;
  if (remainingSek <= 0)
  {
    tsCalStart = 0;
    action = MainAction::Measure;
  }

  lcd.setCursor(0, 1);
  lcd.print("Kalibrierung ");
  lcd.print(String(remainingSek));
  lcd.print("s   ");
}

void executeAction()
{
  if (action == MainAction::Calibrate)
    startCalibration();
  else if (action == MainAction::Info)
    printInfo();
}

void setup()
{
  // on board LED on
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);

  // setup menu buttons
  pinMode(BTN1, INPUT_PULLUP);
  bouncer1.attach(BTN1);
  // bouncer1.interval(15);
  pinMode(BTN2, INPUT_PULLDOWN_16);
  bouncer2.attach(BTN2);
  // bouncer2.interval(15);

  // Serial Monitor
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== CO2-Ampel ===");
  Serial.println();

  // LCD
  lcd.init();
  lcd.createChar(2, characterUpperTwo);
  lcd.createChar(3, characterWlan);
  lcd.createChar(4, characterBtnL);
  lcd.createChar(5, characterBtnR);
  lcd.setCursor(0, 0);
  lcd.print("CO\x02-Ampel");
  lcd.setCursor(0, 1);
  lcd.print("Initialisierung");
  lcd.backlight();

  // Sensor
  softSerial.begin(9600);
  sensor.begin(softSerial);
  sensor.autoCalibration();

  // init done, on board LED off
  digitalWrite(LED_BUILTIN, 1);
}

void loop()
{
  tsNow = millis();

  bouncer1.update();
  if (bouncer1.fell())
  {
    changeAction();
  }

  bouncer2.update();
  if (bouncer2.fell())
  {
    executeAction();
  }

  if ((action == MainAction::Calibrate) && (tsCalStart > 0))
  {
    updateCalibrationProgress();
  }
  else
  {
    updateSensorData();
  }
}