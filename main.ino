#define REMOTEXY_MODE__ESP8266_HARDSERIAL_POINT
#include "dht.h"  
#include <LiquidCrystal.h>
#include <RemoteXY.h>
#include <SPI.h>
#include <MFRC522.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define VAKA 230597 // current active COVID-19 case number goes here 

// RemoteXY connection settings

#define REMOTEXY_SERIAL Serial
#define REMOTEXY_SERIAL_SPEED 115200
#define REMOTEXY_WIFI_SSID "dien" // Wifi SSID goes here
#define REMOTEXY_WIFI_PASSWORD "123456789" // Wifi password goes here
#define REMOTEXY_SERVER_PORT 6377
#define DHT11_PIN 2
#define fan 3
#define SS_PIN 10
#define RST_PIN 9


MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

double iqpt = 0, Q = 1, risk = 0;
const int rs = 8, en = 15, d4 = 16, d5 = 17, d6 = 18, d7 = 19, AOUTpin = A0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
dht DHT;
int ppm = 0, fanSpeed = 0, sensitivity = 5, currentPeople = 0, sensor1Initial, sensor2Initial, timeoutCounter = 0;
char info[32], buffer[24];
int sensor1[] = {6, 7}, sensor2[] = {4, 5};
String sequence = "";


// RemoteXY configurate
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 65 bytes
  { 255,0,0,42,0,58,0,16,186,0,129,0,7,10,84,6,22,67,79,50,
  44,32,78,101,109,44,32,83,99,107,44,32,89,108,99,44,32,82,105,115,
  107,44,32,70,97,110,0,67,4,6,19,85,8,186,22,29,67,1,27,35,
  45,10,22,26,13 }; 
  
// this structure defines all the variables and events of your control interface
struct {

    // output variables
  char co2[29];  // string UTF8 end zero 
  char riskLevel[13];  // string UTF8 end zero 

  // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;
#pragma pack(pop)


void setup()
{
  RemoteXY_Init();
  sensor1Initial = measureDistance(sensor1);
  sensor2Initial = measureDistance(sensor2);

  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  initialPrint();
  pinMode(fan, OUTPUT);
  delay(1000);
}

int loopCount = 0;
int chk = 0;

void loop()
{
  RemoteXY_Handler ();

  if (loopCount % 10 == 0) 
  {
    ppm = analogRead(AOUTpin);
    chk = DHT.read11(DHT11_PIN);
  }

  counter();
  readCard();
  calculateRisk();
  printLCD();
  calculateRiskLevel();
  calculateFanLevel();
  setFanSpeed();  
  transmitInfo();
   
  loopCount++;
}

void counter()
{
  //Read ultrasonic sensors
  int sensor1Val = measureDistance(sensor1);
  int sensor2Val = measureDistance(sensor2);

  //Process the data
  if (sensor1Val < sensor1Initial - sensitivity && sequence.charAt(0) != '1') {
    sequence += "1";
  } else if (sensor2Val < sensor2Initial - sensitivity && sequence.charAt(0) != '2') {
    sequence += "2";
  }

  if (sequence.equals("12")) {
    if (currentPeople != 0) currentPeople--;
    sequence = "";
    delay(500);
  }


  //Resets the sequence if it is invalid or timeouts
  if (sequence.length() > 2 || sequence.equals("11") || sequence.equals("22") ||  sequence.equals("21") || timeoutCounter > 10) {
    sequence = "";
  }

  if (sequence.length() == 1) { //
    timeoutCounter++;
  } else {
    timeoutCounter = 0;
  }

}


//a[0] = echo, a[1] = trig
int measureDistance(int a[]) {
  pinMode(a[1], OUTPUT);
  digitalWrite(a[1], LOW);
  delayMicroseconds(2);
  digitalWrite(a[1], HIGH);
  delayMicroseconds(10);
  digitalWrite(a[1], LOW);
  pinMode(a[0], INPUT);
  long duration = pulseIn(a[0], HIGH, 100000);
  return duration / 29 / 2;
}

void readCard() {
  // Reset the loop if no new card present on the sensor
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( rfid.PICC_ReadCardSerial()) currentPeople++;
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void initialPrint()
{
  lcd.begin(16, 2);
  lcd.print("CO2,Nm,Sc,Ylc:0");
  lcd.setCursor(0, 1);
  lcd.print("S1:");
  lcd.print(sensor1Initial);
  lcd.print(", S2:");
  lcd.print(sensor2Initial);
}

void printLCD()
{
  lcd.setCursor(0, 0);
  lcd.print("CO2,Nm,Sc,R,Y:");
  lcd.print(currentPeople);
  lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print(ppm);
  lcd.print(",");
  lcd.print(DHT.humidity, 0);
  lcd.print(",");
  lcd.print(DHT.temperature, 0);
  lcd.print(",");
  lcd.print(risk, 4);
  lcd.print(" ");
}

void calculateRiskLevel()
{
  if (risk > 0.2) 
  {
    fanSpeed = 3;
    strcpy(RemoteXY.riskLevel, "YUKSEK RISK");
  }
  else if (risk > 0.1) 
  {
    fanSpeed = 2;
    strcpy(RemoteXY.riskLevel, "RISKLI");
  }
  else if (risk > 0.05)
  {
    fanSpeed = 1;
    strcpy(RemoteXY.riskLevel, "DUSUK RISK");
  }

}

void calculateFanLevel()
{
  if ( ppm > 900 || DHT.humidity > 80 || DHT.temperature > 37 || currentPeople > 47 ) fanSpeed = 3;
  else if ( ( ppm > 800 || DHT.humidity > 70 || DHT.temperature > 32 || currentPeople > 31 ) && fanSpeed < 3) fanSpeed = 2;
  else if ( ( ppm > 600 || DHT.humidity > 60 || DHT.temperature > 27 || currentPeople > 23 ) && fanSpeed < 2) fanSpeed = 1;
  else if (risk < 0.05)
  {
    fanSpeed = 0;
    strcpy(RemoteXY.riskLevel, "GUVENLI");
  }

}

void calculateRisk()
{
  iqpt = -17.408 * ( currentPeople * ( 0.0027231490 ));
  Q = 5000 / ppm;
  risk = 1 - exp(iqpt / Q);
}

void setFanSpeed()
{
  if (fanSpeed == 3) analogWrite(fan, 255);
  if (fanSpeed == 2) analogWrite(fan, 192);
  if (fanSpeed == 1) analogWrite(fan, 128);
  if (fanSpeed == 0) analogWrite(fan, 0);
}

void transmitInfo()
{
  itoa(ppm, buffer, 10);
  strcpy(info, buffer);
  strcat(info, ", ");
  itoa(DHT.humidity, buffer, 10);
  strcat(info, buffer);
  strcat(info, "%, ");
  itoa(DHT.temperature, buffer, 10);
  strcat(info, buffer);
  strcat(info, "C, ");
  itoa(currentPeople, buffer, 10);
  strcat(info, buffer);
  strcat(info, ", ");
  dtostrf(risk, 6, 4, buffer); 
  strcat(info, buffer);
  strcat(info, ", ");
  itoa(fanSpeed, buffer, 10);
  strcat(info, buffer);
  strcpy(RemoteXY.co2, info);
}