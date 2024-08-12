

#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x3F, 16, 2);

//need to change********
const char* ssid = <wifi ssid>;
const char* password = <wifi password>;
String bicycleID=<bicycle id>;
String tagID=<rfid tag id>;
int lockDelay=3000;
int webUpdateDelay=8000;
long rentTime=0;
String IP= <local network ip or api end-point>;
//***********************

//pins esp8266***********
constexpr uint8_t RST_PIN = D3; //rfid    
constexpr uint8_t SS_PIN = D4; //rfid
constexpr uint8_t R1 = D0;//lock relay pin
constexpr uint8_t R2 = A0;//buzzer relay pin
int BUZZER=D8;
int rentFlag=1;
//***********************

long prev_time=0;
long prev_ok=0;
int flag=1;
int ok=1;
int scanned;
int status_fixed;
int ok_flag=1;
String status="status not updated";
int retry=1;
int rent=0;
int identification;
int rfidFlag;

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

String tag;

void setup() {
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  pinMode(R1,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  digitalWrite(R1,HIGH);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("  Welcome ");

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  
  Serial.println("Connected to the WiFi network");
  lcd.setCursor(0,1);
  lcd.print(" Wifi connected ");
  delay(2000);
  lcd.clear();
}

void loop() {
  if(check_status()){
    if(rent){
      identification=1;
      Serial.println("Unlocked*********");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("  Rent Successfull ");
      digitalWrite(R1,LOW);
      delay(3000);
      digitalWrite(R1,HIGH);
      rent=0;
      rentFlag=0;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("  Rented ");
    }

    //start scan rfid
    scanned=read_rfid();
    Serial.println("");Serial.println("Scanning rfid");
    if(!scanned){
      scanned=read_rfid();
    }

    //unlock the lock
    if(scanned && ok){
      rfidFlag=1;
      digitalWrite(BUZZER,LOW);
      Serial.println("Unlocked Physically");
      prev_time=millis();
      ok=0;
      ok_flag=0;
      retry=1;
    }

    //when tag not scaning
    if(!scanned && !ok && (millis()-prev_time)<=webUpdateDelay-2000){
      digitalWrite(BUZZER,HIGH);
      if(rfidFlag){
        rfidFlag=0;
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("  Identification failed ");
      }
      retry=0;
      ok_flag=1;
    }

    //handle lock update api
    if((millis()-prev_time)>=webUpdateDelay && !ok && retry){
      if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
        retry=1;
        HTTPClient http;
        WiFiClient wifi;
        http.begin(wifi,"http://"+IP+"/api/user/bicycles/lock?bicycle_id="+bicycleID); //Specify the URL
        int httpCode = http.GET();                                        //Make the request
      
        if (httpCode > 0) { //Check for the returning code
            String payload = http.getString();
            Serial.println(httpCode);
            JSONVar myObject = JSON.parse(payload);
            if(String(httpCode)=="200"){
              identification=1;
              Serial.println("Database updated to Locked");
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("  Return Successfull ");
              delay(2000);
              lcd.clear();
              rentFlag=1;
              ok_flag=1;
            }
          }
      
        else {
          Serial.println("Error on HTTP request");
        }
      
        http.end(); //Free the resources
      }
      digitalWrite(BUZZER,HIGH);
      delay(200);
      digitalWrite(BUZZER,LOW);
    }
  }
  else{
    lcd.setCursor(0,0);
    lcd.print("  Rent now ");
  }
  delay(500);
}



//check status using api
int check_status(){
  String error;
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;
    WiFiClient wifi;

    http.begin(wifi,"http://"+IP+"/api/user/bicycles/lock/status?bicycle_id="+bicycleID); //Specify the URL
    int httpCode = http.GET();                                        
  
    if (httpCode > 0) { //Check for the returning code
  
        String payload = http.getString();
        JSONVar myObject = JSON.parse(payload);
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        if(String(httpCode)=="200"){
          status=String(myObject["lock_status"]);
        }
      }
  
    else {
      Serial.println("Error on HTTP request");
    }
  
    http.end(); //Free the resources
  }
  Serial.print("Lock status in the Databse: ");
  Serial.println(status);

  if(status=="locked"){
    return 0;
  }
  else{
    if(ok_flag){
      ok=1;
      if(rentFlag){
        rent=1;
        rentTime=millis();
      }
    }
    return 1;
  }
}


//handle rfid reading
int read_rfid(){
  tag = "";
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      tag += rfid.uid.uidByte[i];
    }
    //Serial.println(tag);
    if(tag==tagID){
      if(identification){
        identification=0;
        lcd.setCursor(0,0);
        lcd.print("  Identification successfull ");
      }
      return 1;

    }
    else{
      if(identification){
        identification=1;
        lcd.setCursor(0,0);
        lcd.print("  Identification failed ");
      }
      digitalWrite(BUZZER,HIGH);
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  return 0;
}


