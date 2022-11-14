#include "Nextion.h"
#include <time.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ThingSpeak.h>
#include <FirebaseESP32.h>

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;

unsigned long myChannel_ID = 1479047;
const char* myWriteAPIKEY = "62EBAD4PMT50SBD4";

#define FIREBASE_HOST "smartfarm-dht22-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "L5QLq1Sc7RPELOIcSsxH1Wd21tnA7v05SGBFD3YE"
#define DHTPIN 23
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
FirebaseData firebasedata;
String path = "/ESP32_Device";
WiFiClient client;

#define LEDC_F 12
#define LEDPUMP 13
#define COOLING_FAN 14
#define PUMP 27
#define OPUMP 33
#define OCOOLING 26
#define LED 2

char buffer[100] = {0};
const int SWcooling = 18;
const int SWpump = 19;
const int SWmode = 21;
const int setHumid = 34;
const int setTemp = 35;

int humidvalue = 0, tempvalue = 0;
int pumpState = 0, modeState = 0, coolingState = 0;
int h, t; // humidity, temperature

NexText t0 = NexText(0 , 1, "t0"); // temp
NexText t1 = NexText(0, 2, "t1"); // humid
NexText t2 = NexText(0, 3, "t2"); // set temp
NexText t3 = NexText(0, 4, "t3"); // set humid
NexText t5 = NexText(0, 8, "t5"); // hour
NexText t6 = NexText(0, 9, "t6"); // min
NexPicture p0 = NexPicture(0, 5, "p0"); // cooling
NexPicture p1 = NexPicture(0, 6, "p1"); // water pump

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
//---Reset WiFi open//
  //WiFiManager wm;
  //wm.resetSettings();
  //ESP.restart();
  connectwifi();
//------------------//
  init();
  initsetup();
  connectTime();
}

void connectwifi() {
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  if ( !wm.autoConnect("SMARTFARM_WiFi_Config")) {
    ESP.restart();
    delay(1000);
  }
}

void init() {
  Serial.begin(115200);
  nexInit();
  dht.begin();
  ThingSpeak.begin(client);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

void initsetup() {
  pinMode(SWcooling, INPUT);
  pinMode(SWpump, INPUT);
  pinMode(SWmode, INPUT);
  delay(1000);
  pinMode(OPUMP, OUTPUT);
  pinMode(OCOOLING, OUTPUT);
  pinMode(LEDC_F, OUTPUT);
  pinMode(LEDPUMP, OUTPUT);
  pinMode(COOLING_FAN, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(LED, OUTPUT);
  delay(1000);
  digitalWrite(LEDC_F, LOW);
  digitalWrite(COOLING_FAN, LOW);
  digitalWrite(PUMP, LOW);
  digitalWrite(LEDPUMP, LOW);
  digitalWrite(OPUMP, LOW);
  digitalWrite(OCOOLING, LOW);
  digitalWrite(LED, LOW);
}

void connectTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  LocalTime();
}

int state = 0;
unsigned long previous1 = 0;
unsigned long previous2 = 0;
void loop() {
  if (millis() - previous1 >= 10000) {
    sensor();
    nextion_firebase_thingspeak();
    previous1 = millis();
  }
  if (millis() - previous2 >= 60000) {
    LocalTime();
    previous2 = millis();
  }
  //ดึงค่า Firebase.getInt(firebasedata, path + "/SetTemp", testnum);
  //ส่งค่า Firebase.setInt(firebasedata, path + "/Testtemp", testnum);
  Firebase.getInt(firebasedata, path + "/State", state);
  func_mode();
  manual_mb();
}

int c_state, p_state;
void manual_mb() {
  if (state == 1) {
    Firebase.getInt(firebasedata, path + "/CoolingState", c_state);
    Firebase.getInt(firebasedata, path + "/PumpState", p_state);
    if (c_state == 0) {
      digitalWrite(COOLING_FAN, LOW);
      digitalWrite(LEDC_F, LOW);
      p0.setPic(0);
    }
    else {
      digitalWrite(COOLING_FAN, HIGH);
      digitalWrite(LEDC_F, HIGH);
      p0.setPic(1);
    }
    if (p_state == 0) {
      digitalWrite(PUMP, LOW);
      digitalWrite(LEDPUMP, LOW);
      p1.setPic(0);
    }
    else {
      digitalWrite(PUMP, HIGH);
      digitalWrite(LEDPUMP, HIGH);
      p1.setPic(1);
    }
  }
}

bool stateclear = 0;
void func_mode() {
  if (state == 0) {
    modeState = digitalRead(SWmode);
    if (modeState == 0) {
      digitalWrite(OCOOLING, HIGH);
      digitalWrite(LED, LOW);
      coolingState = digitalRead(SWcooling);
      if (coolingState == 0) {
        digitalWrite(COOLING_FAN, LOW);
        digitalWrite(LEDC_F, LOW);
        p0.setPic(0);
      }
      else {
        digitalWrite(COOLING_FAN, HIGH);
        digitalWrite(LEDC_F, HIGH);
        p0.setPic(1);
      }
      pumpState = digitalRead(SWpump);
      digitalWrite(OPUMP, HIGH);
      if (pumpState == 0) {
        digitalWrite(PUMP, LOW);
        digitalWrite(LEDPUMP, LOW);
        p1.setPic(0);
      }
      else {
        digitalWrite(PUMP, HIGH);
        digitalWrite(LEDPUMP, HIGH);
        p1.setPic(1);
      }
      stateclear = 1;
    }
    else {
      digitalWrite(LED, HIGH);
      if (stateclear == 1) {
        digitalWrite(LEDC_F, LOW);
        digitalWrite(LEDPUMP, LOW);
        digitalWrite(COOLING_FAN, LOW);
        digitalWrite(OPUMP, LOW);
        digitalWrite(OCOOLING, LOW);
        p0.setPic(0);
        p1.setPic(0);
        stateclear = 0;
      }
      if (h < humidvalue) {
        digitalWrite(PUMP, HIGH);
        digitalWrite(LEDPUMP, HIGH);
        p1.setPic(1);
      }
      else if (h > humidvalue + 9) {
        digitalWrite(PUMP, LOW);
        digitalWrite(LEDPUMP, LOW);
        p1.setPic(0);
      }
      if (t > tempvalue) {
        digitalWrite(COOLING_FAN, HIGH);
        digitalWrite(LEDC_F, HIGH);
        p0.setPic(1);
      }
      else if (t < tempvalue - 2) {
        digitalWrite(COOLING_FAN, LOW);
        digitalWrite(LEDC_F, LOW);
        p0.setPic(0);
      }
    }
  }
}

void sensor() {
  humidvalue = map(analogRead(setHumid), 0, 4095, 70, 85);
  tempvalue = map(analogRead(setTemp), 0, 4095, 25, 35);
  h = dht.readHumidity();
  t = dht.readTemperature();

  if (h > 100 || t > 80) {
    h = 0;
    t = 0;
  }
}

void nextion_firebase_thingspeak() {
  if (digitalRead(PUMP) == HIGH) { Firebase.setInt(firebasedata, path + "/PumpState", 1); }
  else { Firebase.setInt(firebasedata, path + "/PumpState", 0); }
  if (digitalRead(COOLING_FAN) == HIGH) { Firebase.setInt(firebasedata, path + "/CoolingState", 1); }
  else { Firebase.setInt(firebasedata, path + "/CoolingState", 0); }
  if (modeState == 0) { Firebase.setInt(firebasedata, path + "/ModeState", 0); }
  else { Firebase.setInt(firebasedata, path + "/ModeState", 1); }
  Firebase.setInt(firebasedata, path + "/Humidity", h);
  Firebase.setInt(firebasedata, path + "/Temperature", t);
  Firebase.setInt(firebasedata, path + "/SetHumid", humidvalue);
  Firebase.setInt(firebasedata, path + "/SetTemp", tempvalue);
//------- send nextion ---------//  
  memset(buffer, 0, sizeof(buffer));
  itoa(t, buffer, 10);  // data temperature
  t0.setText(buffer);
  
  memset(buffer, 0, sizeof(buffer));
  itoa(h, buffer, 10);  // data humidity
  t1.setText(buffer);
  
  memset(buffer, 0, sizeof(buffer));
  itoa(tempvalue, buffer, 10);  // data tempvalue
  t2.setText(buffer);
  
  memset(buffer, 0, sizeof(buffer));
  itoa(humidvalue, buffer, 10); //  data humidityvalue
  t3.setText(buffer);
  
  memset(buffer, 0, sizeof(buffer));
  itoa(timeinfo.tm_hour, buffer, 10); //  data hour
  t5.setText(buffer);
  
  memset(buffer, 0, sizeof(buffer));
  itoa(timeinfo.tm_min, buffer, 10);  //  data min
  t6.setText(buffer);
//------- send iot thingspeak database -------//
  ThingSpeak.setField(1, h);  //  data humidity
  ThingSpeak.setField(2, t);  //  data temperature
  int x = ThingSpeak.writeFields(myChannel_ID, myWriteAPIKEY);
  if (x == 200) {
    Serial.println("Channel update successful.");
  }else { Serial.println("Problem updating channel. HTTP error code " + String(x)); }
}

void LocalTime() {
  if (!getLocalTime(&timeinfo)) {
    return;
  }
}
