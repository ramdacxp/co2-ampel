#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "MHZ19.h"
#include <SoftwareSerial.h>
// #include <Bounce2.h>

#define CO2LEVEL_MAX_OK 800
#define CO2LEVEL_MAX_WARN 1000

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
// #define BTN1 D1MINI_D3
// #define BTN2 D1MINI_D8
// #define BTN_DEBOUNCETIME 50

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

// Bounce bouncer1 = Bounce();

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

const char *co2ToString(int co2)
{
  return (co2 <= CO2LEVEL_MAX_OK)
             ? "OK"
             : (co2 <= CO2LEVEL_MAX_WARN)
                   ? "Warnung"
                   : "Kritisch";
}

void readSensor()
{
  // update every 2sec only
  static unsigned long lastUpdate = 0;
  unsigned long currentTimestamp = millis();
  if (currentTimestamp - lastUpdate < 10000)
  {
    return;
  }
  lastUpdate = currentTimestamp;

  Serial.println("Reading sensor ...");

  // read data
  lcd.noBacklight();
  int co2 = sensor.getCO2();
  float temp = sensor.getTemperature(true);

  Serial.print("- CO² (ppm): ");
  Serial.println(co2);
  Serial.print("- Temperature (°C): ");
  Serial.println(temp);
  Serial.print("- ErrorCode: ");
  Serial.println(sensor.errorCode);

  lcd.setCursor(0, 1);
  lcd.print(String(co2));
  lcd.print("    ");

  lcd.setCursor(8, 1);
  lcd.print(co2ToString(co2));
  lcd.print("      ");

  lcd.setCursor(10, 0);
  lcd.print(String(temp, 1));
  lcd.print("\xDF");
  lcd.print("C");
  lcd.backlight();
}

enum MainAction
{
  Measure,
  Calibrate,
  Info
};

static MainAction action = MainAction::Measure;

void printAction()
{
  lcd.setCursor(0, 0);
  if (action == MainAction::Measure)
    lcd.print("Messen   ");
  else if (action == MainAction::Calibrate)
    lcd.print("Kalibr.  ");
  else if (action == MainAction::Info)
    lcd.print("Infos    ");
}

void printName()
{
  lcd.setCursor(0, 0);
  lcd.print("CO\x02-Ampel");
}

void toggleAction()
{
  if (action == MainAction::Measure)
    action = MainAction::Calibrate;
  else if (action == MainAction::Calibrate)
    action = MainAction::Info;
  else
    action = MainAction::Measure;

  printAction();
}

void setup()
{
  // on board LED on
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);

  // setup menu buttons
  // pinMode(BTN1, INPUT_PULLUP);
  // bouncer1.attach(BTN1);
  // bouncer1.interval(15);

  // Serial Monitor
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== CO2-Ampel ===");
  Serial.println();

  // LCD
  lcd.init();
  lcd.createChar(2, characterUpperTwo);
  lcd.createChar(3, characterWlan);
  printName();
  lcd.backlight();

  // Sensor
  softSerial.begin(9600);
  sensor.begin(softSerial);
  sensor.autoCalibration();

  // lcd.setCursor(15, 0);
  // lcd.write(3);
  // lcd.setCursor(15, 0);
  // lcd.blink();

  // init done, on board LED off
  digitalWrite(LED_BUILTIN, 1);
}

void loop()
{
  // bouncer1.update();
  // if (bouncer1.fell())
  // {
  //   toggleAction();
  // }

  if (action == MainAction::Measure)
  {
    readSensor();
  }
}