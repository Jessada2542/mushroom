#include "Nextion.h"
#include <time.h>
#include <WiFi.h>
#include <ThingSpeak.h>
#include <FirebaseESP32.h>
#include <BlynkSimpleEsp32.h>
#include <TridentTD_ESP32NVS.h>
#include <Wire.h>
#include "DFRobot_SHT20.h"

const char* ssid = "THANTHIP_IOT";
const char* pass = "64627977";

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;

unsigned long myChannel_ID = 1571363;
const char* myWriteAPIKEY = "IZXUPRGX9LOXXIQV";

#define FIREBASE_HOST "mushroom-1a019-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "HwGlIbmvuagHLlbSLgKoR0ZZ206pIeVzK6C3fU9o"
FirebaseData firebaseData;
DFRobot_SHT20 sht20;
WiFiClient client;
BlynkTimer timer;

#define fan_pin     12
#define cooling_pin 14
#define pump_pin    27

#define status_manual      25
#define status_auto        26
#define status_connect     33
#define status_on_cooling  13
#define status_off_cooling 32
#define status_on_pump     2
#define status_off_pump    23

char buffer[100] = {0};
//----- switch button
const int sw_cooling = 5; 
const int sw_pump = 19;
const int sw_mode = 18;
//----- caborn sensor
const int co2_pin = 4;

int curren_coolingstate = LOW, previous_coolingstate = LOW;
int curren_pumpstate = LOW, previous_pumpstate = LOW;
int curren_modestate = LOW, previous_modestate = LOW;
int state_mode = 0, state_cooling = 0, state_pump = 0;

int humidvalue = 0, tempvalue = 0;//25-35 & 70-80
int co2value = 0;

NexText settemp = NexText(0 , 2, "settemp"); // settemp
NexText tempp = NexText(0, 3, "tempp"); // temp
NexText sethumid = NexText(0, 4, "sethumid"); // set humid
NexText humidd = NexText(0, 5, "humidd"); // humid
NexText gass = NexText(0, 10, "gass"); // co2
NexText timee = NexText(0, 8, "timee"); // time
NexText setgass = NexText(0, 12, "setgass");  //set co2
NexPicture coolingg = NexPicture(0, 6, "coolingg"); // cooling
NexPicture pumpp = NexPicture(0, 7, "pumpp"); // water pump
NexPicture modd = NexPicture(0, 9, "modd"); // mode
NexText t1 = NexText(0 , 13, "t1");
NexText t2 = NexText(0 , 14, "t2");

int _humid = 0;
int _temp = 0;
int humid, temp, co2;

void setup() {
  init_sht20();
  init_nextion();
  init_nvs();
  init_setup();
  connect_wifi();
  connect_firebase();
  connect_thingspeak();
  connect_Time();
  func_Timer();
  clear_pumpcooling_off();
}

void init_sht20() {
  sht20.initSHT20(); // Init SHT20 Sensor
  delay(100);
  sht20.checkSHT20(); // Check SHT20 Sensor
}

void init_nextion() {
  nexInit();
}

void init_nvs() {
  NVS.begin();

  humidvalue = NVS.getInt("A");
  tempvalue = NVS.getInt("B");
  co2value = NVS.getInt("C");
  state_mode = NVS.getInt("M");
}

void init_setup() {
  pinMode(sw_cooling, INPUT);
  pinMode(sw_pump, INPUT);
  pinMode(sw_mode, INPUT);
  pinMode(co2_pin, INPUT_PULLUP);
  delay(1000);
  pinMode(fan_pin, OUTPUT);
  pinMode(cooling_pin, OUTPUT);
  pinMode(pump_pin, OUTPUT);
  pinMode(status_manual, OUTPUT);
  pinMode(status_auto, OUTPUT);
  pinMode(status_connect, OUTPUT);
  pinMode(status_on_cooling, OUTPUT);
  pinMode(status_off_cooling, OUTPUT);
  pinMode(status_on_pump, OUTPUT);
  pinMode(status_off_pump, OUTPUT);
  delay(1000);
  digitalWrite(fan_pin, LOW);
  digitalWrite(cooling_pin, LOW);
  digitalWrite(pump_pin, LOW);
  digitalWrite(status_manual, LOW);
  digitalWrite(status_auto, LOW);
  digitalWrite(status_connect, LOW);
  digitalWrite(status_on_cooling, LOW);
  digitalWrite(status_off_cooling, LOW);
  digitalWrite(status_on_pump, LOW);
  digitalWrite(status_off_pump, LOW);
}

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    i++;
    if (i >= 15) {
      break;
    }
  }
}

void connect_firebase() {
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void connect_thingspeak() {
  ThingSpeak.begin(client);
}

void connect_Time() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  LocalTime();
}

void func_Timer() {
  timer.setInterval(1000L, sw_button);
  timer.setInterval(10000L, get_firebase);
  timer.setInterval(10000L, read_sensor);
  timer.setInterval(15000L, nextion_set_firebase);
  timer.setInterval(50000L, system_auto);
  timer.setInterval(60000L, LocalTime);
  timer.setInterval(60000L, checkwifi);
  timer.setInterval(600000L, send_thingspeak);
}

void clear_pumpcooling_off() {
  state_mode = 0;
  state_cooling = 0;
  state_pump = 0;
  digitalWrite(status_manual, !state_mode);
  digitalWrite(status_auto, state_mode);
  digitalWrite(status_on_cooling, state_cooling);
  digitalWrite(status_off_cooling, !state_cooling);
  digitalWrite(status_on_pump, state_pump);
  digitalWrite(status_off_pump, !state_pump);
  Firebase.setInt(firebaseData, "/cooling", state_cooling);
  Firebase.setInt(firebaseData, "/pump", state_pump);
}

int count = 0;
void loop() {
  timer.run();
  if (digitalRead(fan_pin) == HIGH) {
    count++;
  }
}

int _humidity() {
  int humidity = sht20.readHumidity();
  if (humidity > 0 && humidity < 100) {
    _humid = humidity;
    return humidity;
  }
  else {
    return _humid;
  }
}

int _temperature() {
  int temperature = sht20.readTemperature();
  if (temperature > 0 && temperature < 100) {
    _temp = temperature;
    return temperature;
  }
  else {
    return _temp;
  }
}

int _gascarbon() {
  while (digitalRead(co2_pin) == LOW) {};
  long t0 = millis();
  while (digitalRead(co2_pin) == HIGH) {};
  long t1 = millis();
  while (digitalRead(co2_pin) == LOW) {};
  long t2 = millis();

  long time_high = t1 - t0;
  long time_low = t2 - t1;
  long ppm = 5000L * (time_high - 2) / (time_high + time_low - 4);
  while (digitalRead(co2_pin) == HIGH) {};
  delay(10);
  if (ppm > 0 && ppm < 5000) {
    return int(ppm);
  }
  else {
    return 0;
  }
}

void read_sensor() {
  humid = _humidity();
  Serial.print(humid);
  delay(50);
  temp = _temperature();
  Serial.println(temp);
  delay(50);
  co2 = _gascarbon();
}

void sw_button() {
  curren_modestate = digitalRead(sw_mode);
  if (curren_modestate != previous_modestate && previous_modestate == HIGH) {
    state_mode = !state_mode;
    if (state_mode == 1) {
      digitalWrite(status_manual, LOW);
      digitalWrite(status_auto, HIGH);
      modd.setPic(4);
    }
    else {
      digitalWrite(status_manual, HIGH);
      digitalWrite(status_auto, LOW);
      modd.setPic(3);
    }
    Firebase.setInt(firebaseData, "/mode", state_mode);
  }
  previous_modestate = curren_modestate;

  curren_coolingstate = digitalRead(sw_cooling);
  if (curren_coolingstate != previous_coolingstate && previous_coolingstate == HIGH) {
    if (state_mode == 0) {
      state_cooling = !state_cooling;
      if (state_cooling == 1) {
        digitalWrite(fan_pin, HIGH);
        digitalWrite(status_on_cooling, HIGH);
        digitalWrite(status_off_cooling, LOW);
        delay(10000);
        digitalWrite(cooling_pin, HIGH);
        coolingg.setPic(2);
      }
      else {
        digitalWrite(fan_pin, LOW);
        digitalWrite(cooling_pin, LOW);
        digitalWrite(status_on_cooling, LOW);
        digitalWrite(status_off_cooling, HIGH);
        coolingg.setPic(1);
      }
      Firebase.setInt(firebaseData, "/cooling", state_cooling);
    }
  }
  previous_coolingstate = curren_coolingstate;

  curren_pumpstate = digitalRead(sw_pump);
  if (curren_pumpstate != previous_pumpstate && previous_pumpstate == HIGH) {
    if (state_mode == 0) {
      state_pump = !state_pump;
      if (state_pump == 1) {
        digitalWrite(pump_pin, HIGH);
        digitalWrite(status_on_pump, HIGH);
        digitalWrite(status_off_pump, LOW);
        pumpp.setPic(2);
      }
      else {
        digitalWrite(pump_pin, LOW);
        digitalWrite(status_on_pump, LOW);
        digitalWrite(status_off_pump, HIGH);
        pumpp.setPic(1);
      }
    }
    Firebase.setInt(firebaseData, "/pump", state_pump);
  }
  previous_pumpstate = curren_pumpstate;
}

void get_firebase() {
  if (Firebase.getInt(firebaseData, "/mode")) {
    if (firebaseData.dataType() == "int") {
      NVS.setInt("M", firebaseData.intData());
      state_mode = NVS.getInt("M");
      if (state_mode == 1) {
        digitalWrite(status_manual, LOW);
        digitalWrite(status_auto, HIGH);
        modd.setPic(4);
      }
      else {
        digitalWrite(status_manual, HIGH);
        digitalWrite(status_auto, LOW);
        modd.setPic(3);
      }
    }
  }
  if (Firebase.getInt(firebaseData, "/cooling")) {
    if (firebaseData.dataType() == "int") {
      if (firebaseData.intData() == 1) {
        digitalWrite(fan_pin, HIGH);
        digitalWrite(status_on_cooling, HIGH);
        digitalWrite(status_off_cooling, LOW);
        coolingg.setPic(2);
        if (count >= 10) {
          digitalWrite(cooling_pin, HIGH);
        }
      }
      else {
        digitalWrite(fan_pin, LOW);
        digitalWrite(cooling_pin, LOW);
        digitalWrite(status_on_cooling, LOW);
        digitalWrite(status_off_cooling, HIGH);
        coolingg.setPic(1);
        count = 0;
      }
    }
  }
  if (Firebase.getInt(firebaseData, "/pump")) {
    if (firebaseData.dataType() == "int") {
      digitalWrite(pump_pin, firebaseData.intData());
      digitalWrite(status_on_pump, firebaseData.intData());
      digitalWrite(status_off_pump, !firebaseData.intData());
      if (firebaseData.intData() == 1) {
        pumpp.setPic(2);
      }
      else {
        pumpp.setPic(1);
      }
    }
  }
  if (Firebase.getInt(firebaseData, "/sethumidity")) {
    if (firebaseData.dataType() == "int") {
      NVS.setInt("A", firebaseData.intData());
      humidvalue = NVS.getInt("A");
    }
  }
  if (Firebase.getInt(firebaseData, "/settemperature")) {
    if (firebaseData.dataType() == "int") {
      NVS.setInt("B", firebaseData.intData());
      tempvalue = NVS.getInt("B");
    }
  }
  if (Firebase.getInt(firebaseData, "/setgascarbon")) {
    if (firebaseData.dataType() == "int") {
      NVS.setInt("C", firebaseData.intData());
      co2value = NVS.getInt("C");
    }
  }
}

void system_auto() {
  if (state_mode == 1) {
    int h = timeinfo.tm_hour;
    int m = timeinfo.tm_min;
    //--------------------- pump -----------------------//
    if (humid != 0) {
      if (humid < humidvalue && h >= 7 && h <= 17 && m == 1) {
        digitalWrite(pump_pin, HIGH);
        digitalWrite(status_on_pump, HIGH);
        digitalWrite(status_off_pump, LOW);
        pumpp.setPic(2);
        Firebase.setInt(firebaseData, "/pump", 1);
      }
      else if (humid > humidvalue + 5 && m > 0 && m < 10) {
        digitalWrite(pump_pin, LOW);
        digitalWrite(status_on_pump, LOW);
        digitalWrite(status_off_pump, HIGH);
        pumpp.setPic(1);
        Firebase.setInt(firebaseData, "/pump", 0);
      }
      if (humid < humidvalue && h >= 7 && h <= 17 && m == 31) {
        digitalWrite(pump_pin, HIGH);
        digitalWrite(status_on_pump, HIGH);
        digitalWrite(status_off_pump, LOW);
        pumpp.setPic(2);
        Firebase.setInt(firebaseData, "/pump", 1);
      }
      else if (humid > humidvalue && m > 31 && m < 40) {
        digitalWrite(pump_pin, LOW);
        digitalWrite(status_on_pump, LOW);
        digitalWrite(status_off_pump, HIGH);
        pumpp.setPic(1);
        Firebase.setInt(firebaseData, "/pump", 0);
      }
      if (digitalRead(pump_pin) == HIGH && m == 10) {
        digitalWrite(pump_pin, LOW);
        digitalWrite(status_on_pump, LOW);
        digitalWrite(status_off_pump, HIGH);
        pumpp.setPic(1);
        Firebase.setInt(firebaseData, "/pump", 0);
      }
      if (digitalRead(pump_pin) == HIGH && m == 40) {
        digitalWrite(pump_pin, LOW);
        digitalWrite(status_on_pump, LOW);
        digitalWrite(status_off_pump, HIGH);
        pumpp.setPic(1);
        Firebase.setInt(firebaseData, "/pump", 0);
      }
      digitalWrite(status_connect, LOW);
    }
    else {
      digitalWrite(pump_pin, LOW);
      digitalWrite(status_on_pump, LOW);
      digitalWrite(status_off_pump, HIGH);
      pumpp.setPic(1);
      Firebase.setInt(firebaseData, "/pump", 0);
      digitalWrite(status_connect, HIGH);
    }
    //---------------------- cooling ------------------------//
    if (temp != 0) {
      if (temp > tempvalue && h >= 7 && h <= 17 && m > 10 && m < 30) {
        digitalWrite(fan_pin, HIGH);
        digitalWrite(status_on_cooling, HIGH);
        digitalWrite(status_off_cooling, LOW);
        Firebase.setInt(firebaseData, "/cooling", 1);
        delay(10000);
        digitalWrite(cooling_pin, HIGH);
        coolingg.setPic(2);
      }
      else if (temp < tempvalue - 1 && m > 10 && m < 30) {
        digitalWrite(fan_pin, LOW);
        digitalWrite(cooling_pin, LOW);
        digitalWrite(status_on_cooling, LOW);
        digitalWrite(status_off_cooling, HIGH);
        coolingg.setPic(1);
        Firebase.setInt(firebaseData, "/cooling", 0);
      }
      if (co2 > co2value && h >= 7 && h <= 17 && m > 40 && m < 60) {
        digitalWrite(fan_pin, HIGH);
        digitalWrite(status_on_cooling, HIGH);
        digitalWrite(status_off_cooling, LOW);
        Firebase.setInt(firebaseData, "/cooling", 1);
        delay(10000);
        digitalWrite(cooling_pin, HIGH);
        coolingg.setPic(2);
      }
      else if (co2 < co2value - 100 && m > 40 && m < 60) {
        digitalWrite(fan_pin, LOW);
        digitalWrite(cooling_pin, LOW);
        digitalWrite(status_on_cooling, LOW);
        digitalWrite(status_off_cooling, HIGH);
        coolingg.setPic(1);
        Firebase.setInt(firebaseData, "/cooling", 0);
      }
      if (digitalRead(fan_pin) == HIGH && digitalRead(cooling_pin) ==  HIGH && m == 0) {
        digitalWrite(fan_pin, LOW);
        digitalWrite(cooling_pin, LOW);
        digitalWrite(status_on_cooling, LOW);
        digitalWrite(status_off_cooling, HIGH);
        coolingg.setPic(1);
        Firebase.setInt(firebaseData, "/cooling", 0);
      }
      if (digitalRead(fan_pin) == HIGH && digitalRead(cooling_pin) ==  HIGH && m == 30) {
        digitalWrite(fan_pin, LOW);
        digitalWrite(cooling_pin, LOW);
        digitalWrite(status_on_cooling, LOW);
        digitalWrite(status_off_cooling, HIGH);
        coolingg.setPic(1);
        Firebase.setInt(firebaseData, "/cooling", 0);
      }
      digitalWrite(status_connect, LOW);
    }
    else {
      digitalWrite(fan_pin, LOW);
      digitalWrite(cooling_pin, LOW);
      digitalWrite(status_on_cooling, LOW);
      digitalWrite(status_off_cooling, HIGH);
      coolingg.setPic(1);
      Firebase.setInt(firebaseData, "/cooling", 0);
      digitalWrite(status_connect, HIGH);
    }
    pump_cooling_on();
    pump_cooling_off();
  }
}

String H = "--", M = "--";
void nextion_set_firebase() {
  //------- send nextion ---------//
  memset(buffer, 0, sizeof(buffer));
  itoa(temp, buffer, 10);  // data temperature
  tempp.setText(buffer);

  memset(buffer, 0, sizeof(buffer));
  itoa(humid, buffer, 10);  // data humidity
  humidd.setText(buffer);

  memset(buffer, 0, sizeof(buffer));
  itoa(co2, buffer, 10);  // data gas
  gass.setText(buffer);

  memset(buffer, 0, sizeof(buffer));
  itoa(tempvalue, buffer, 10);  // data tempvalue
  settemp.setText(buffer);

  memset(buffer, 0, sizeof(buffer));
  itoa(humidvalue, buffer, 10); //  data humidityvalue
  sethumid.setText(buffer);

  memset(buffer, 0, sizeof(buffer));
  itoa(co2value, buffer, 10); //  data gas
  setgass.setText(buffer);

  const char* h = H.c_str();
  const char* m = M.c_str();
  t1.setText(h);
  t2.setText(m);

  //---------- set firebase ---------------//
  Firebase.setInt(firebaseData, "/humidity", humid);
  Firebase.setInt(firebaseData, "/temperature", temp);
  Firebase.setInt(firebaseData, "/gascarbon", co2);
}

void send_thingspeak() {
  if (humid != 0) {
    ThingSpeak.setField(1, humid);  //  data humidity
  }
  else {
    ThingSpeak.setField(1, 0);
  }
  if (temp != 0) {
    ThingSpeak.setField(2, temp);  //  data temperature
  }
  else {
    ThingSpeak.setField(2, 0);
  }
  ThingSpeak.setField(5, co2);
  status_database();
}

boolean pumpon = true, coolingon = true;
void pump_cooling_on() {
  if (digitalRead(pump_pin) != HIGH) {
    pumpon = false;
  }
  else if (digitalRead(pump_pin) == HIGH && pumpon != true) {
    if (humid != 0) {
      ThingSpeak.setField(1, humid);
      ThingSpeak.setField(3, 1);
    }
    else {
      ThingSpeak.setField(1, 0);
      ThingSpeak.setField(3, 1);
    }
    if (temp != 0) {
      ThingSpeak.setField(2, temp);
    }
    else {
      ThingSpeak.setField(2, 0);
    }
    ThingSpeak.setField(5, co2);
    pumpon = true;
    status_database();
  }
  if (digitalRead(cooling_pin) != HIGH) {
    coolingon = false;
  }
  else if (digitalRead(cooling_pin) == HIGH && coolingon != true) {
    if (temp != 0) {
      ThingSpeak.setField(2, temp);
      ThingSpeak.setField(4, 1);
    }
    else {
      ThingSpeak.setField(2, 0);
      ThingSpeak.setField(4, 1);
    }
    if (humid != 0) {
      ThingSpeak.setField(1, humid);
    }
    else {
      ThingSpeak.setField(1, 0);
    }
    ThingSpeak.setField(5, co2);
    coolingon = true;
    status_database();
  }
}

boolean pumpoff = true, coolingoff = true;
void pump_cooling_off() {
  if (digitalRead(pump_pin) != LOW) {
    pumpoff = false;
  }
  else if (digitalRead(pump_pin) == LOW && pumpoff != true) {
    if (humid != 0) {
      ThingSpeak.setField(1, humid);
      ThingSpeak.setField(3, 0);
    }
    else {
      ThingSpeak.setField(1, 0);
      ThingSpeak.setField(3, 0);
    }
    if (temp != 0) {
      ThingSpeak.setField(2, temp);
    }
    else {
      ThingSpeak.setField(2, 0);
    }
    ThingSpeak.setField(5, co2);
    pumpoff = true;
    status_database();
  }
  if (digitalRead(cooling_pin) != LOW) {
    coolingoff = false;
  }
  else if (digitalRead(cooling_pin) == LOW && coolingoff != true) {
    if (temp != 0) {
      ThingSpeak.setField(2, temp);
      ThingSpeak.setField(4, 0);
    }
    else {
      ThingSpeak.setField(2, 0);
      ThingSpeak.setField(4, 0);
    }
    if (humid != 0) {
      ThingSpeak.setField(1, humid);
    }
    else {
      ThingSpeak.setField(1, 0);
    }
    ThingSpeak.setField(5, co2);
    coolingoff = true;
    status_database();
  }
}

void status_database() {
  int code = ThingSpeak.writeFields(myChannel_ID, myWriteAPIKEY);
  if (code == 200) {
    Serial.println("Channel update successful.");
    Firebase.setString(firebaseData, "/database", "success");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(code));
    Firebase.setString(firebaseData, "/database", "fail");
  }
}

void LocalTime() {
  if (!getLocalTime(&timeinfo)) {
    return;
  }
  H = (timeinfo.tm_hour < 10 ? "0" + String(timeinfo.tm_hour) : String(timeinfo.tm_hour));
  M = (timeinfo.tm_min < 10 ? "0" + String(timeinfo.tm_min) : String(timeinfo.tm_min));
}

void checkwifi() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(status_connect, HIGH);
  }
  else {
    digitalWrite(status_connect, LOW);
  }
}
