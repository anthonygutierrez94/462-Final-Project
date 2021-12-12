#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <dht11.h>
#include <IRremote.h>

#define DHT11_PIN 3
#define PIR_PIN 2
#define LED_PIN 13
#define IR_PIN 7

#define BACKLIGHT_DURATION 8000
#define BUTTON_DURATION 6000
//#define TEMPERATURE_INTERVAL 15*60*1000
#define TEMPERATURE_INTERVAL 1000
#define LCD_INTERVAL 2000

#define IR_0 -384368896
#define IR_1 -217252096
#define IR_2 -417792256
#define IR_3 -1587609856
#define IR_4 -150405376

enum IRMode {
 DEFAULT_MODE, 
 MOTION_COUNTER = IR_0,
 BUTTON_COUNTER = IR_1,
 TEMP_ALERT_COUNTER = IR_2,
 TEMPERATURE = IR_3,
 CURRENT_TIME = IR_4
};

const long BUTTON_MODE[] = {MOTION_COUNTER, BUTTON_COUNTER, TEMP_ALERT_COUNTER, TEMPERATURE, CURRENT_TIME};

LiquidCrystal_I2C lcd(0x27,16,2);
RTC_DS1307 rtc;
dht11 dht;

int tempAlertCounter = 0;
int motionCounter = 0;
int buttonCounter = 0;
double temp = 0;
IRMode currentMode = DEFAULT_MODE;
unsigned long modeButtonLastPressed = 0;
unsigned long tempNextTime = 0;
unsigned long lcdNextTime = 0;
unsigned long turnBackLightOffAt = 0;
bool backLightOn = false;

const int modeLength = sizeof(BUTTON_MODE) / sizeof(long);

double fahrenheit(double celsius){
  return (1.8 * celsius) + 32;
}

void setup () {
  Serial.begin(9600);
  lcd.init();
 
  IrReceiver.begin(IR_PIN, true);
  //irrecv.blink13(true);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("RTC is NOT running, let's set the time!");
  }
}

void readTemperature(){
    DateTime currentDate = rtc.now();
    dht.read(DHT11_PIN);
    double oldTemp = temp;
    temp = fahrenheit(dht.temperature);
    Serial.print("temperature;");
    Serial.println(temp);
    if (temp < 95 && oldTemp >= 95){
      Serial.println("TemperatureDrop");
      tempAlertCounter++;
    }
}

void displayLCD(){
  DateTime currentDateTime = rtc.now();
  unsigned long now = millis();
  if (now >= (modeButtonLastPressed + BUTTON_DURATION) && (currentMode != DEFAULT_MODE)){
    currentMode = DEFAULT_MODE;
  }
  if (backLightOn && now>= turnBackLightOffAt){
    lcd.noBacklight();
    backLightOn = false;
  }
  lcd.clear();
  lcd.setCursor(0,0);
  switch (currentMode){
    case DEFAULT_MODE:
    {
        lcd.print(currentDateTime.hour(), DEC);
        lcd.print(":");
        lcd.print(currentDateTime.minute(), DEC);
        lcd.print(":");
        lcd.print(currentDateTime.second(), DEC);
        lcd.setCursor(9,0);
        lcd.print(fahrenheit(dht.temperature));
        lcd.print(" F");
        lcd.setCursor(9,1);
        lcd.print(dht.temperature);
        lcd.print(" C");
        lcd.setCursor(0,1);
        lcd.print(dht.humidity);
        lcd.print("%H");
        break;
    }
    case MOTION_COUNTER:
    {
        lcd.print("motion counter: ");
        lcd.setCursor(0,1);
        lcd.print(motionCounter);
        break;
    }   
    case BUTTON_COUNTER:
    {
        lcd.print("button counter: ");
        lcd.setCursor(0,1);
        lcd.print(buttonCounter);
        break;
    }
    case TEMP_ALERT_COUNTER:
    {
        lcd.print("temp alerts: ");
        lcd.setCursor(0,1);
        lcd.print(tempAlertCounter);
        break;
    }
    case TEMPERATURE:
    {
        lcd.print("Temperature: ");
        lcd.setCursor(0,1);
        lcd.print(temp);
        break;
    }
    case CURRENT_TIME:
    {
        lcd.print("Current Time: ");
        lcd.setCursor(0,1);
        lcd.print(currentDateTime.hour(), DEC);
        lcd.print(":");
        lcd.print(currentDateTime.minute(), DEC);
        lcd.print(":");
        lcd.print(currentDateTime.second(), DEC);
        break;
    }
  }
  
}

void loop () {
    
    unsigned long now = millis();
    if (now >= lcdNextTime){
      displayLCD();
      lcdNextTime = now + LCD_INTERVAL;
    }

    if (now >= tempNextTime){
      readTemperature();
      tempNextTime = now + TEMPERATURE_INTERVAL; //(15*60*1000) 15 minutes in millis
    }
    
    //read IR data
    if (IrReceiver.decode())// Returns 0 if no data ready, 1 if data ready.     
    {
        long readResults = IrReceiver.decodedIRData.decodedRawData;   
        
        IrReceiver.resume(); // Restart the ISR state machine and Receive the next value    
        for (int i=0; i<modeLength; i++){
          if (BUTTON_MODE[i] == readResults){
            if(!backLightOn){
              backLightOn = true;
              lcd.backlight();
            }
            turnBackLightOffAt = now + BACKLIGHT_DURATION;
            currentMode = BUTTON_MODE[i];
            modeButtonLastPressed = now;
            buttonCounter++;
            break;
          }
        }
    }
    
    //read motion sensor data
    if (digitalRead(PIR_PIN) == HIGH) {
      if(!backLightOn){
        backLightOn = true;
        motionCounter++;
        Serial.println("motion");
        lcd.backlight();
      }
      turnBackLightOffAt = now + BACKLIGHT_DURATION;
    }
 }

