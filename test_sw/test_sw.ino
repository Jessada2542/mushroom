#include <WiFi.h>
#include <FirebaseESP32.h>
#include <BlynkSimpleEsp32.h>

BlynkTimer timer;

const char* ssid = "IOT";
const char* password = "20212021";

#define FIREBASE_HOST "mushroom-dd474-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "qCkGoGwGi0jmW0xqgQx6dM1fif9x7BT8Q9IWd21U"
FirebaseData firebaseData;

const int pin = 35;
int currenbuttonstate = LOW;
int previousbuttonstate = LOW;
int istate = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(pin, INPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  timer.setInterval(10000L, get_firebase);
}

void loop() {
  timer.run();
  // put your main code here, to run repeatedly:
  currenbuttonstate = digitalRead(pin);
  if (currenbuttonstate != previousbuttonstate && previousbuttonstate == HIGH) {
    istate = !istate;
    Firebase.setInt(firebaseData, "test", istate);
    Serial.print("state: ");
    Serial.print(istate);
    Serial.println();
    delay(500);
  }
  previousbuttonstate = currenbuttonstate;
}

void get_firebase() {
   if (Firebase.getInt(firebaseData, "test")) {
    if (firebaseData.dataType() == "int") {
      int s = firebaseData.intData();
      istate = s;
      Serial.print("stateApp: ");
      Serial.print(istate);
      Serial.println();
    }
  }
}
