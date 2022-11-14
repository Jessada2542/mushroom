const int pwmpin = 4;

void setup() {
  pinMode(pwmpin, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("start");

}

int i = 0;
void loop() {
  int ppm_pwm = gas_pwm();
  i++;
  Serial.print(String(i) + " - digital: ");
  Serial.print(ppm_pwm);
  Serial.println();
  delay(10000);
}

int gas_pwm() {
  while (digitalRead(pwmpin) == LOW) {};
  long t0 = millis();
  while (digitalRead(pwmpin) == HIGH) {};
  long t1 = millis();
  while (digitalRead(pwmpin) == LOW) {};
  long t2 = millis();
  long th = t1 - t0;
  long tl = t2 - t1;
  long ppm = 5000L * (th - 2) / (th + tl - 4);
  while (digitalRead(pwmpin) == HIGH) {};
  delay(10);
  return int(ppm);
}
