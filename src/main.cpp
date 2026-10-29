
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LittleFS.h>
#include <Adafruit_SHT31.h>
#include <math.h>

// ======================================================
// ESP32 MONITOR
// ESP32 Gas & Environmental Monitoring System
// ======================================================

const char* AP_SSID = "GasMonitor-ESP32";
const char* AP_PASSWORD = "GasMonitor32";

constexpr uint8_t MQ3_PIN = 32;
constexpr uint8_t MQ137_PIN = 33;
constexpr uint8_t MQ138_PIN = 34;
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;

constexpr int MQ3_THRESHOLD = 1800;
constexpr int MQ137_THRESHOLD = 1800;
constexpr int MQ138_THRESHOLD = 1800;
constexpr unsigned long READ_INTERVAL_MS = 2000;

WebServer server(80);
Adafruit_SHT31 sht31 = Adafruit_SHT31();

bool sht31Available = false;
bool littleFSAvailable = false;
uint8_t sht31Address = 0x44;

struct SensorCalibration {
  const char* targetGas;
  const char* units;
  bool circuitVerified;
  float supplyVoltageV;
  float loadResistanceOhms;
  bool dividerConfigured;
  float dividerRatio;
  bool r0Calibrated;
  float r0Ohms;
  bool curveReady;
  float curveA;
  float curveB;
  float minRange;
  float maxRange;
  const char* calibrationStatus;
  const char* calibrationDate;
  const char* calibrationSource;
};

SensorCalibration mq3Config = {
  "ethanol",
  "ppm",
  false,
  NAN,
  NAN,
  false,
  NAN,
  false,
  NAN,
  false,
  NAN,
  NAN,
  NAN,
  NAN,
  "UNCALIBRATED",
  "UNSPECIFIED",
  "Requires verified circuit and reference gas calibration"
};

SensorCalibration mq137Config = {
  "ammonia (NH3)",
  "ppm",
  false,
  NAN,
  NAN,
  false,
  NAN,
  false,
  NAN,
  false,
  NAN,
  NAN,
  NAN,
  NAN,
  "UNCALIBRATED",
  "UNSPECIFIED",
  "Requires verified circuit and reference gas calibration"
};

SensorCalibration mq138Config = {
  "VOC target gas required",
  "ppm",
  false,
  NAN,
  NAN,
  false,
  NAN,
  false,
  NAN,
  false,
  NAN,
  NAN,
  NAN,
  NAN,
  "UNCALIBRATED",
  "UNSPECIFIED",
  "Requires explicit target-gas selection and calibration"
};

int mq3Raw = 0;
int mq137Raw = 0;
int mq138Raw = 0;

float mq3Volts = NAN;
float mq137Volts = NAN;
float mq138Volts = NAN;
float mq3SensorNodeVoltage = NAN;
float mq137SensorNodeVoltage = NAN;
float mq138SensorNodeVoltage = NAN;
float mq3Resistance = NAN;
float mq137Resistance = NAN;
float mq138Resistance = NAN;
float mq3RsRatio = NAN;
float mq137RsRatio = NAN;
float mq138RsRatio = NAN;
float mq3Ppm = NAN;
float mq137Ppm = NAN;
float mq138Ppm = NAN;

const char* mq3CalibrationStatus = "UNCALIBRATED";
const char* mq137CalibrationStatus = "UNCALIBRATED";
const char* mq138CalibrationStatus = "UNCALIBRATED";

float temperatureC = NAN;
float humidityRH = NAN;
unsigned long lastReadTime = 0;

bool hasValidPositive(float value) {
  return isfinite(value) && value > 0.0f;
}

String jsonNumberOrNull(float value, int decimals = 3) {
  if (!isfinite(value)) {
    return String("null");
  }
  return String(value, decimals);
}

void updateGasSensor(
  float adcVoltage,
  SensorCalibration config,
  float& sensorNodeVoltage,
  float& sensorResistance,
  float& rsRatio,
  float& estimatedPpm,
  const char*& calibrationStatus
) {
  sensorNodeVoltage = NAN;
  sensorResistance = NAN;
  rsRatio = NAN;
  estimatedPpm = NAN;
  calibrationStatus = "UNCALIBRATED";

  if (!isfinite(adcVoltage) || adcVoltage <= 0.0f) {
    return;
  }

  if (config.circuitVerified &&
      hasValidPositive(config.supplyVoltageV) &&
      hasValidPositive(config.loadResistanceOhms)) {
    float nodeVoltage = adcVoltage;
    if (config.dividerConfigured &&
        hasValidPositive(config.dividerRatio)) {
      nodeVoltage = adcVoltage / config.dividerRatio;
    }

    if (hasValidPositive(nodeVoltage) && nodeVoltage < config.supplyVoltageV) {
      sensorNodeVoltage = nodeVoltage;
      sensorResistance = config.loadResistanceOhms *
        ((config.supplyVoltageV / nodeVoltage) - 1.0f);

      if (isfinite(sensorResistance) && sensorResistance > 0.0f) {
        calibrationStatus = "CIRCUIT VERIFIED";
      }
    } else {
      calibrationStatus = "CIRCUIT UNVERIFIED";
    }
  } else {
    calibrationStatus = "CIRCUIT UNVERIFIED";
  }

  if (config.r0Calibrated &&
      hasValidPositive(config.r0Ohms) &&
      isfinite(sensorResistance) &&
      sensorResistance > 0.0f) {
    rsRatio = sensorResistance / config.r0Ohms;
    if (rsRatio > 0.0f && isfinite(rsRatio)) {
      calibrationStatus = "R0 CALIBRATED";
    }
  }

  if (config.curveReady &&
      isfinite(config.curveA) &&
      isfinite(config.curveB) &&
      isfinite(rsRatio) &&
      rsRatio > 0.0f) {
    float candidatePpm = config.curveA * pow(rsRatio, config.curveB);
    if (isfinite(candidatePpm) && candidatePpm >= 0.0f) {
      estimatedPpm = candidatePpm;
      if (isfinite(config.minRange) && estimatedPpm < config.minRange) {
        calibrationStatus = "BELOW CALIBRATION RANGE";
      } else if (isfinite(config.maxRange) && estimatedPpm > config.maxRange) {
        calibrationStatus = "ABOVE CALIBRATION RANGE";
      } else {
        calibrationStatus = "CALIBRATED";
      }
    }
  }
}

String buildSensorJson(
  const char* key,
  const char* name,
  const SensorCalibration& config,
  int raw,
  float adcVoltage,
  float sensorNodeVoltage,
  float sensorResistance,
  float rsRatio,
  float estimatedPpm,
  const char* calibrationStatus,
  float threshold
) {
  String json;
  json.reserve(220);

  json += "{";
  json += "\"id\":\""; json += key; json += "\",";
  json += "\"name\":\""; json += name; json += "\",";
  json += "\"targetGas\":\""; json += config.targetGas; json += "\",";
  json += "\"units\":\""; json += config.units; json += "\",";
  json += "\"raw\":"; json += String(raw);
  json += ",\"adcVoltage\":"; json += jsonNumberOrNull(adcVoltage, 3);
  json += ",\"sensorNodeVoltage\":"; json += jsonNumberOrNull(sensorNodeVoltage, 3);
  json += ",\"sensorResistance\":"; json += isfinite(sensorResistance) ? String(sensorResistance, 2) : String("null");
  json += ",\"rsRatio\":"; json += isfinite(rsRatio) ? String(rsRatio, 4) : String("null");
  json += ",\"ppm\":"; json += isfinite(estimatedPpm) ? String(estimatedPpm, 2) : String("null");
  json += ",\"calibrationStatus\":\""; json += calibrationStatus; json += "\",";
  json += "\"calibrationDate\":\""; json += config.calibrationDate; json += "\",";
  json += "\"source\":\""; json += config.calibrationSource; json += "\",";
  json += "\"threshold\":"; json += isfinite(threshold) ? String(threshold, 2) : String("null");
  json += ",\"thresholdUnit\":\"ppm\"";
  json += "}";
  return json;
}

void readSensors() {
  mq3Raw = analogRead(MQ3_PIN);
  mq137Raw = analogRead(MQ137_PIN);
  mq138Raw = analogRead(MQ138_PIN);

  mq3Volts = analogReadMilliVolts(MQ3_PIN) / 1000.0f;
  mq137Volts = analogReadMilliVolts(MQ137_PIN) / 1000.0f;
  mq138Volts = analogReadMilliVolts(MQ138_PIN) / 1000.0f;

  updateGasSensor(mq3Volts, mq3Config, mq3SensorNodeVoltage, mq3Resistance, mq3RsRatio, mq3Ppm, mq3CalibrationStatus);
  updateGasSensor(mq137Volts, mq137Config, mq137SensorNodeVoltage, mq137Resistance, mq137RsRatio, mq137Ppm, mq137CalibrationStatus);
  updateGasSensor(mq138Volts, mq138Config, mq138SensorNodeVoltage, mq138Resistance, mq138RsRatio, mq138Ppm, mq138CalibrationStatus);

  if (sht31Available) {
    temperatureC = sht31.readTemperature();
    humidityRH = sht31.readHumidity();
  } else {
    temperatureC = NAN;
    humidityRH = NAN;
  }
}

void printReadings() {
  Serial.println();
  Serial.println("========== ESP32 MONITOR ==========");

  Serial.printf("MQ-3   : ADC=%d, ADC pin voltage=%.3f V, status=%s\n",
                mq3Raw, mq3Volts, mq3CalibrationStatus);
  Serial.printf("MQ-137 : ADC=%d, ADC pin voltage=%.3f V, status=%s\n",
                mq137Raw, mq137Volts, mq137CalibrationStatus);
  Serial.printf("MQ-138 : ADC=%d, ADC pin voltage=%.3f V, status=%s\n",
                mq138Raw, mq138Volts, mq138CalibrationStatus);

  if (sht31Available && isfinite(temperatureC) && isfinite(humidityRH)) {
    Serial.printf("SHT31 Temperature: %.1f C\n", temperatureC);
    Serial.printf("SHT31 Humidity   : %.1f %%\n", humidityRH);
  } else {
    Serial.println("SHT31: No valid reading");
  }

  Serial.println("Prototype gas readings remain calibration-aware and are not certified gas measurements.");
  Serial.println("===================================");
}

void handleAPI() {
  String json;
  json.reserve(700);

  json = "{";
  json += "\"status\":\"ok\",";
  json += "\"source\":\"live\",";

  json += "\"mq3\":"; json += String(mq3Raw);
  json += ",\"mq137\":"; json += String(mq137Raw);
  json += ",\"mq138\":"; json += String(mq138Raw);

  json += ",\"v3\":"; json += jsonNumberOrNull(mq3Volts, 3);
  json += ",\"v137\":"; json += jsonNumberOrNull(mq137Volts, 3);
  json += ",\"v138\":"; json += jsonNumberOrNull(mq138Volts, 3);

  json += ",\"rs3\":"; json += isfinite(mq3Resistance) ? String(mq3Resistance, 2) : String("null");
  json += ",\"rs137\":"; json += isfinite(mq137Resistance) ? String(mq137Resistance, 2) : String("null");
  json += ",\"rs138\":"; json += isfinite(mq138Resistance) ? String(mq138Resistance, 2) : String("null");

  json += ",\"ratio3\":"; json += isfinite(mq3RsRatio) ? String(mq3RsRatio, 4) : String("null");
  json += ",\"ratio137\":"; json += isfinite(mq137RsRatio) ? String(mq137RsRatio, 4) : String("null");
  json += ",\"ratio138\":"; json += isfinite(mq138RsRatio) ? String(mq138RsRatio, 4) : String("null");

  json += ",\"ppm3\":"; json += isfinite(mq3Ppm) ? String(mq3Ppm, 2) : String("null");
  json += ",\"ppm137\":"; json += isfinite(mq137Ppm) ? String(mq137Ppm, 2) : String("null");
  json += ",\"ppm138\":"; json += isfinite(mq138Ppm) ? String(mq138Ppm, 2) : String("null");

  json += ",\"temp\":"; json += isfinite(temperatureC) ? String(temperatureC, 1) : String("null");
  json += ",\"hum\":"; json += isfinite(humidityRH) ? String(humidityRH, 1) : String("null");

  json += ",\"t3\":"; json += String(MQ3_THRESHOLD);
  json += ",\"t137\":"; json += String(MQ137_THRESHOLD);
  json += ",\"t138\":"; json += String(MQ138_THRESHOLD);
  json += ",\"sht31\":"; json += sht31Available ? "true" : "false";

  json += ",\"sensors\":[";
  json += buildSensorJson("mq3", "MQ-3", mq3Config, mq3Raw, mq3Volts, mq3SensorNodeVoltage, mq3Resistance, mq3RsRatio, mq3Ppm, mq3CalibrationStatus, MQ3_THRESHOLD);
  json += ",";
  json += buildSensorJson("mq137", "MQ-137", mq137Config, mq137Raw, mq137Volts, mq137SensorNodeVoltage, mq137Resistance, mq137RsRatio, mq137Ppm, mq137CalibrationStatus, MQ137_THRESHOLD);
  json += ",";
  json += buildSensorJson("mq138", "MQ-138", mq138Config, mq138Raw, mq138Volts, mq138SensorNodeVoltage, mq138Resistance, mq138RsRatio, mq138Ppm, mq138CalibrationStatus, MQ138_THRESHOLD);
  json += "]}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleHealth() {
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"sht31\":"; json += sht31Available ? "true" : "false";
  json += ",\"filesystem\":"; json += littleFSAvailable ? "true" : "false";
  json += ",\"ip\":\""; json += WiFi.softAPIP().toString(); json += "\"";
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void serveFile(const char* path, const char* contentType) {
  if (!littleFSAvailable) {
    server.send(503, "text/plain", "LittleFS unavailable. Check the filesystem upload.");
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    server.send(404, "text/plain", "File not found");
    return;
  }

  server.sendHeader("Cache-Control", "no-cache");
  server.streamFile(file, contentType);
  file.close();
}

void configureWebServer() {
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/", HTTP_GET, []() { serveFile("/index.html", "text/html"); });
  server.on("/style.css", HTTP_GET, []() { serveFile("/style.css", "text/css"); });
  server.on("/app.js", HTTP_GET, []() { serveFile("/app.js", "application/javascript"); });
  server.onNotFound([]() { server.send(404, "text/plain", "Endpoint not found"); });

  server.begin();
  Serial.println("Web server started.");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Starting ESP32 Monitor...");

  analogReadResolution(12);
  analogSetPinAttenuation(MQ3_PIN, ADC_11db);
  analogSetPinAttenuation(MQ137_PIN, ADC_11db);
  analogSetPinAttenuation(MQ138_PIN, ADC_11db);

  Wire.begin(SDA_PIN, SCL_PIN);

  sht31Available = sht31.begin(0x44);
  sht31Address = 0x44;
  if (!sht31Available) {
    sht31Available = sht31.begin(0x45);
    sht31Address = 0x45;
  }

  if (sht31Available) {
    Serial.printf("SHT31 detected at I2C address 0x%02X\n", sht31Address);
  } else {
    Serial.println("SHT31 not detected. Check VCC, GND, SDA, SCL and address.");
  }

  littleFSAvailable = LittleFS.begin(false);
  if (littleFSAvailable) {
    Serial.println("LittleFS mounted.");
    if (LittleFS.exists("/index.html") &&
        LittleFS.exists("/style.css") &&
        LittleFS.exists("/app.js")) {
      Serial.println("All dashboard files found.");
    } else {
      Serial.println("Dashboard files missing. Build and upload the filesystem.");
    }
  } else {
    Serial.println("LittleFS mount failed. Check filesystem configuration and upload.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (apStarted) {
    Serial.println("Wi-Fi access point started.");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("Dashboard: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Wi-Fi access point failed.");
  }

  configureWebServer();
  readSensors();
  printReadings();
  lastReadTime = millis();
}

void loop() {
  server.handleClient();

  if (millis() - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = millis();
    readSensors();
    printReadings();
  }

  delay(2);
}
