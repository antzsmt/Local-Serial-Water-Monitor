/*
Connections:
1. DHT11 & Free Life v1.0 board
   S   --- Digital Pin9
   VCC --- VCC
   GND --- GND

2. Moisture Sensor(Humidity) & Free Life v1.0 board
   1S   --- Analog Pin2
   2GND --- GND
   3VCC --- VCC

3. DS18B20 Temperature Sensor
   DQ   --- Digital Pin8 (with 4.7kΩ pull-up to VCC)
   VCC  --- 5V
   GND  --- GND
*/

#include "AutoWatering.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#define MaxTemperature 100
#define SensorTest 1
#define CarbonTest 0

// DS18B20 setup
#define ONE_WIRE_BUS 8
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// EEPROM addresses
#define EEPROM_ADDR_DRY 0
#define EEPROM_ADDR_WET 2

// Calibration storage
int dryBaseline;
int wetBaseline;

// Calibration button
#define CALIBRATE_BUTTON 10

AutoWatering flower;

// --- Auto-detect functions ---
int detectSensorRange() {
  int maxSample = 0;
  for (int i = 0; i < 20; i++) {
    int val = analogRead(A2);
    if (val > maxSample) maxSample = val;
    delay(10);
  }

  if (maxSample < 700) {
    Serial.println("Detected Z6379A (0–3V output).");
    return 614;
  } else if (maxSample < 900) {
    Serial.println("Detected SEN0114 (0–4.2V output).");
    return 860;
  } else {
    Serial.println("Detected Z6379 (0–5V output).");
    return 1023;
  }
}

void initCalibration() {
  int sensorMax = detectSensorRange();

  if (sensorMax <= 614) {
    dryBaseline = 600;
    wetBaseline = 200;
  } else if (sensorMax <= 860) {
    dryBaseline = 700;
    wetBaseline = 300;
  } else {
    dryBaseline = 800;
    wetBaseline = 300;
  }

  EEPROM.put(EEPROM_ADDR_DRY, dryBaseline);
  EEPROM.put(EEPROM_ADDR_WET, wetBaseline);

  Serial.print("Calibration defaults set. Dry="); Serial.print(dryBaseline);
  Serial.print(" Wet="); Serial.println(wetBaseline);
}

// --- Setup ---
void setup() {
  flower.Initialization();
  Serial.begin(115200);
  sensors.begin();
  pinMode(CALIBRATE_BUTTON, INPUT_PULLUP);

  EEPROM.get(EEPROM_ADDR_DRY, dryBaseline);
  EEPROM.get(EEPROM_ADDR_WET, wetBaseline);

  if (dryBaseline < 100 || dryBaseline > 1023 ||
      wetBaseline < 100 || wetBaseline > 1023) {
    initCalibration();
  }

  Serial.println("System ready.");
  Serial.println("Commands: CALIBRATE DRY, CALIBRATE WET, SHOW CALIBRATION, RESET CALIBRATION, AUTO DETECT, CALIBRATION TEST, DIAGNOSE, LOG HISTORY, SHOW HISTORY, CLEAR HISTORY, AVG TEST, PUMP TEST ON, PUMP TEST OFF, STATUS");
  Serial.print("Dry baseline: "); Serial.println(dryBaseline);
  Serial.print("Wet baseline: "); Serial.println(wetBaseline);
}

// --- Loop ---
void loop() {
  if (digitalRead(CALIBRATE_BUTTON) == LOW) {
    Serial.println("Button pressed: recording DRY baseline...");
    dryBaseline = analogRead(A2);
    EEPROM.put(EEPROM_ADDR_DRY, dryBaseline);
    Serial.print("New dry baseline saved: "); Serial.println(dryBaseline);
    delay(1000);
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("CALIBRATE DRY")) {
      dryBaseline = analogRead(A2);
      EEPROM.put(EEPROM_ADDR_DRY, dryBaseline);
      Serial.print("Dry baseline saved: "); Serial.println(dryBaseline);

    } else if (cmd.equalsIgnoreCase("CALIBRATE WET")) {
      wetBaseline = analogRead(A2);
      EEPROM.put(EEPROM_ADDR_WET, wetBaseline);
      Serial.print("Wet baseline saved: "); Serial.println(wetBaseline);

    } else if (cmd.equalsIgnoreCase("SHOW CALIBRATION")) {
      Serial.print("Current dry baseline: "); Serial.println(dryBaseline);
      Serial.print("Current wet baseline: "); Serial.println(wetBaseline);

    } else if (cmd.equalsIgnoreCase("RESET CALIBRATION")) {
      dryBaseline = 800;
      wetBaseline = 300;
      EEPROM.put(EEPROM_ADDR_DRY, dryBaseline);
      EEPROM.put(EEPROM_ADDR_WET, wetBaseline);
      Serial.println("Calibration reset to defaults (Dry=800, Wet=300).");

    } else if (cmd.equalsIgnoreCase("AUTO DETECT")) {
      initCalibration();

    } else if (cmd.equalsIgnoreCase("CALIBRATION TEST")) {
      Serial.println("Starting calibration test... (20s)");
      unsigned long start = millis();
      while (millis() - start < 20000) {
        int raw = analogRead(A2);
        int moisture_pct = map(raw, dryBaseline, wetBaseline, 0, 100);
        moisture_pct = constrain(moisture_pct, 0, 100);

        Serial.print("Raw soil ADC: "); Serial.print(raw);
        Serial.print(" | Moisture %: "); Serial.println(moisture_pct);

        delay(1000);
      }
      Serial.println("Calibration test complete.");

    } else if (cmd.equalsIgnoreCase("DIAGNOSE")) {
      Serial.println("----- DIAGNOSTIC SNAPSHOT -----");
      int raw = analogRead(A2);
      int moisture_pct = map(raw, dryBaseline, wetBaseline, 0, 100);
      moisture_pct = constrain(moisture_pct, 0, 100);

      int sensorMax = detectSensorRange();
      if (sensorMax <= 614) Serial.println("Sensor type: Z6379A (0–3V)");
      else if (sensorMax <= 860) Serial.println("Sensor type: SEN0114 (0–4.2V)");
      else Serial.println("Sensor type: Z6379 (0–5V)");

      Serial.print("Dry baseline: "); Serial.println(dryBaseline);
      Serial.print("Wet baseline: "); Serial.println(wetBaseline);
      Serial.print("Raw soil ADC: "); Serial.println(raw);
      Serial.print("Moisture %: "); Serial.println(moisture_pct);
      Serial.println("-------------------------------");

    } else if (cmd.equalsIgnoreCase("LOG HISTORY")) {
      Serial.println("Logging last 5 averaged soil moisture readings to EEPROM...");
      for (int i = 0; i < 5; i++) {
        int avg = getAverageMoisture();
        EEPROM.put(10 + i * 2, avg);
        Serial.print("Logged avg reading "); Serial.print(i + 1);
        Serial.print(": "); Serial.println(avg);
        delay(1000);
      }
      Serial.println("History logging complete. Use SHOW HISTORY to view.");

    } else if (cmd.equalsIgnoreCase("SHOW HISTORY")) {
      Serial.println("----- Moisture History (averaged) -----");
      for (int i = 0; i < 5; i++) {
        int val;
        EEPROM.get(10 + i * 2, val);
        Serial.print("Reading "); Serial.print(i + 1);
        Serial.print(": "); Serial.println(val);
      }
      Serial.println("---------------------------------------");

    } else if (cmd.equalsIgnoreCase("CLEAR HISTORY")) {
      Serial.println("Clearing history...");
      for (int i = 0; i < 5; i++) {
        int zero = 0;
        EEPROM.put(10 + i * 2, zero);
      }
      Serial.println("History cleared.");

    } else if (cmd.equalsIgnoreCase("AVG TEST")) {
      int avg = getAverageMoisture();
      Serial.print("Rolling average moisture: "); Serial.print(avg); Serial.println("%");

    } else if (cmd.equalsIgnoreCase("PUMP TEST ON")) {
      flower.pumpTestOn();

    } else if (cmd.equalsIgnoreCase("PUMP TEST OFF")) {
      flower.pumpOff();

    } else if (cmd.equalsIgnoreCase("STATUS")) {
      printStatus();
    }
  }

  pumpOn(SensorTest, CarbonTest, MaxTemperature);
  delay(2000);
  flower.pumpOff();
  delay(2000);
}

// --- Helpers ---
// --- Rolling average soil moisture (last 5 samples) ---
int getAverageMoisture() {
  const int sampleCount = 5;
  long total = 0;

  for (int i = 0; i < sampleCount; i++) {
    int raw = analogRead(A2);
    int moisture_pct = map(raw, dryBaseline, wetBaseline, 0, 100);
    moisture_pct = constrain(moisture_pct, 0, 100);
    total += moisture_pct;
    delay(50); // small delay between samples
  }

  int avgMoisture = total / sampleCount;
  Serial.print("Average soil moisture % ("); Serial.print(sampleCount);
  Serial.print(" samples): "); Serial.println(avgMoisture);

  return avgMoisture;
}

// --- Soil moisture with calibration (single sample) ---
int getCalibratedMoisture() {
  int raw = analogRead(A2);
  int moisture_pct = map(raw, dryBaseline, wetBaseline, 0, 100);
  moisture_pct = constrain(moisture_pct, 0, 100);
  Serial.print("Raw soil ADC: "); Serial.print(raw);
  Serial.print(" | Moisture %: "); Serial.println(moisture_pct);
  return moisture_pct;
}

// --- Pump control using averaged moisture ---
void pumpOn(int MoiSensor, int Carbon, int Temperature_max) {
  int humidity;
  int humidity_max;
  float dht_t;
  float ds18b20_t;

  // Soil moisture (averaged)
  humidity = getAverageMoisture();
  humidity_max = flower.ADJ_humMax();

  // DHT11 temperature
  dht_t = flower.getTemperature();
  Serial.print("DHT11 Temperature: "); Serial.print(dht_t); Serial.println(" °C");

  // DS18B20 temperature
  sensors.requestTemperatures();
  ds18b20_t = sensors.getTempCByIndex(0);
  bool ds_ok = (ds18b20_t != DEVICE_DISCONNECTED_C && ds18b20_t > -55 && ds18b20_t < 125);
  Serial.print("DS18B20 Temperature: "); Serial.print(ds_ok ? ds18b20_t : -127); Serial.println(" °C");

  float temp_for_logic = ds_ok ? ds18b20_t : dht_t;

  // Reset pump pins
  digitalWrite(6, LOW);
  digitalWrite(5, LOW);
  digitalWrite(7, LOW);
  digitalWrite(4, LOW);

  // Pump logic
  if (humidity <= humidity_max && temp_for_logic <= Temperature_max) {
    if (humidity < 30) {  // only water if soil <30%
      digitalWrite(6, HIGH);
      digitalWrite(5, HIGH);
      Serial.println("Pump is switched on!");
    }
  } else {
    digitalWrite(6, LOW);
    digitalWrite(5, LOW);
  }

  // Ambient humidity from DHT11
  int airHumidity = flower.getHumidity();
  Serial.print("Air Humidity is: "); Serial.print(airHumidity); Serial.println("%");
}

// --- Print one-time snapshot ---
void printStatus() {
  Serial.println("----- STATUS REPORT -----");
  int soil = getCalibratedMoisture();   // single sample for quick snapshot
  int airHumidity = flower.getHumidity();
  float dht_t = flower.getTemperature();
  sensors.requestTemperatures();
  float ds18b20_t = sensors.getTempCByIndex(0);
  bool ds_ok = (ds18b20_t != DEVICE_DISCONNECTED_C && ds18b20_t > -55 && ds18b20_t < 125);

  Serial.print("Soil Moisture: "); Serial.print(soil); Serial.println("%");
  Serial.print("Air Humidity: "); Serial.print(airHumidity); Serial.println("%");
  Serial.print("DHT11 Temp: "); Serial.print(dht_t); Serial.println(" °C");
  Serial.print("DS18B20 Temp: "); Serial.print(ds_ok ? ds18b20_t : -127); Serial.println(" °C");

  // Pump state check
  bool pumpState = (digitalRead(6) == HIGH || digitalRead(5) == HIGH);
  Serial.print("Pump State: "); Serial.println(pumpState ? "ON" : "OFF");
  Serial.println("-------------------------");
}
