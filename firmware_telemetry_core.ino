#include <DHT.h>

// ---------------- Pin map (remap for your own board) ----------------
#define DHT_PIN       15
#define DHT_TYPE      DHT22
#define LDR_PIN       34          // ADC1_CH6, input-only pin
#define TRIG_PIN      5
#define ECHO_PIN      18
#define PWM_LED_PIN   25

// ---------------- PWM config ----------------
#define PWM_FREQ      5000
#define PWM_RES_BITS  8            // 0-255 duty range

DHT dht(DHT_PIN, DHT_TYPE);

// ---------------- Non-blocking task intervals (ms) ----------------
const unsigned long DHT_INTERVAL   = 2200; // DHT22 needs >=2s between reads
const unsigned long LDR_INTERVAL   = 250;
const unsigned long SONIC_INTERVAL = 150;
const unsigned long PRINT_INTERVAL = 1000;

unsigned long lastDHT = 0, lastLDR = 0, lastSonicTrigger = 0, lastPrint = 0;

// ---------------- Rolling "signal matrix": last N samples per sensor ----------------
const int WINDOW = 5;
float tempBuf[WINDOW]  = {0};
float humBuf[WINDOW]   = {0};
float ldrBuf[WINDOW]   = {0};
float sonicBuf[WINDOW] = {0};
int tIdx = 0, hIdx = 0, lIdx = 0, sIdx = 0;

float lastGoodTemp = 25.0, lastGoodHum = 50.0, lastGoodDist = 50.0;
unsigned long errorCount = 0;

// ---------------- Interrupt-driven ultrasonic echo timing ----------------
volatile unsigned long echoRiseTime = 0;
volatile unsigned long echoWidth    = 0;
volatile bool echoReady = false;

void IRAM_ATTR onEchoChange() {
  if (digitalRead(ECHO_PIN) == HIGH) {
    echoRiseTime = micros();
  } else {
    echoWidth = micros() - echoRiseTime;
    echoReady = true;
  }
}

// ---------------- Helpers ----------------
void pushSample(float *buf, int &idx, float value) {
  buf[idx] = value;
  idx = (idx + 1) % WINDOW;
}

float bufAverage(float *buf) {
  float sum = 0;
  for (int i = 0; i < WINDOW; i++) sum += buf[i];
  return sum / WINDOW;
}

// Non-blocking error correction: reject out-of-range/invalid samples and
// hold the last good value instead of retrying or stalling the loop.
float correctReading(float value, float lastGood, float minV, float maxV, bool valid) {
  if (!valid || isnan(value) || value < minV || value > maxV) {
    errorCount++;
    return lastGood;
  }
  return value;
}

void triggerUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), onEchoChange, CHANGE);

  ledcAttach(PWM_LED_PIN, PWM_FREQ, PWM_RES_BITS);

  Serial.println("== Firmware Telemetry Core booting ==");
}

void loop() {
  unsigned long now = millis();

  if (now - lastDHT >= DHT_INTERVAL) {
    lastDHT = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    t = correctReading(t, lastGoodTemp, -40, 80, !isnan(t));
    h = correctReading(h, lastGoodHum, 0, 100, !isnan(h));
    lastGoodTemp = t;
    lastGoodHum  = h;
    pushSample(tempBuf, tIdx, t);
    pushSample(humBuf, hIdx, h);
  }

  if (now - lastLDR >= LDR_INTERVAL) {
    lastLDR = now;
    int raw = analogRead(LDR_PIN);
    float lightPct = (raw / 4095.0) * 100.0;
    pushSample(ldrBuf, lIdx, lightPct);
    int duty = constrain(255 - (int)((lightPct / 100.0) * 255), 0, 255);
    ledcWrite(PWM_LED_PIN, duty);
  }

  if (now - lastSonicTrigger >= SONIC_INTERVAL) {
    lastSonicTrigger = now;
    triggerUltrasonic();
  }
  if (echoReady) {
    noInterrupts();
    unsigned long width = echoWidth;
    echoReady = false;
    interrupts();
    float distCM = width * 0.0343 / 2.0;
    distCM = correctReading(distCM, lastGoodDist, 2, 400, width > 0);
    lastGoodDist = distCM;
    pushSample(sonicBuf, sIdx, distCM);
  }

  if (now - lastPrint >= PRINT_INTERVAL) {
    lastPrint = now;
    Serial.print("{\"t_ms\":");      Serial.print(now);
    Serial.print(",\"temp_C\":");    Serial.print(bufAverage(tempBuf), 1);
    Serial.print(",\"hum_pct\":");   Serial.print(bufAverage(humBuf), 1);
    Serial.print(",\"light_pct\":"); Serial.print(bufAverage(ldrBuf), 1);
    Serial.print(",\"dist_cm\":");   Serial.print(bufAverage(sonicBuf), 1);
    Serial.print(",\"errors\":");    Serial.print(errorCount);
    Serial.println("}");
  }
}
