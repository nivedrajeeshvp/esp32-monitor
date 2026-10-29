
"use strict";

const API_URL = "/api";
const REFRESH_INTERVAL_MS = 2000;
const DEMO_MODE = true;

const THRESHOLDS = {
  mq3: 1800,
  mq137: 1800,
  mq138: 1800
};

const SENSOR_UI = {
  mq3: {
    valueId: "mq3",
    voltageId: "vMq3",
    resistanceId: "rsMq3",
    ratioId: "ratioMq3",
    ppmId: "ppmMq3",
    gasId: "gasMq3",
    barId: "barMq3",
    statusId: "statusMq3"
  },
  mq137: {
    valueId: "mq137",
    voltageId: "vMq137",
    resistanceId: "rsMq137",
    ratioId: "ratioMq137",
    ppmId: "ppmMq137",
    gasId: "gasMq137",
    barId: "barMq137",
    statusId: "statusMq137"
  },
  mq138: {
    valueId: "mq138",
    voltageId: "vMq138",
    resistanceId: "rsMq138",
    ratioId: "ratioMq138",
    ppmId: "ppmMq138",
    gasId: "gasMq138",
    barId: "barMq138",
    statusId: "statusMq138"
  }
};

const connectionStatus = document.getElementById("connectionStatus");
const systemStatus = document.getElementById("systemStatus");
const lastUpdated = document.getElementById("lastUpdated");
const connectionDot = document.querySelector(".pulse-dot");
const dataSourceBadge = document.getElementById("dataSourceBadge");

let consecutiveFailures = 0;
let demoReadings = {
  mq3: 1640,
  mq137: 1320,
  mq138: 1710,
  v3: 1.322,
  v137: 1.064,
  v138: 1.379,
  temp: 24.0,
  hum: 40.0,
  t3: 1800,
  t137: 1800,
  t138: 1800,
  sensors: []
};

function setConnectionState(connected) {
  connectionStatus.textContent = connected ? "ESP32 CONNECTED" : "ESP32 OFFLINE";
  connectionStatus.style.color = connected ? "var(--green)" : "var(--red)";

  if (connectionDot) {
    connectionDot.style.background = connected ? "var(--green)" : "var(--red)";
  }
}

function setLocalDemoState() {
  connectionStatus.textContent = "LOCAL DEMO MODE";
  connectionStatus.style.color = "var(--orange)";

  if (connectionDot) {
    connectionDot.style.background = "var(--orange)";
  }
}

function setDataSource(sourceLabel) {
  if (dataSourceBadge) {
    dataSourceBadge.textContent = sourceLabel;
    dataSourceBadge.style.color =
      sourceLabel === "LIVE ESP32 DATA" ? "var(--green)" : "var(--orange)";
  }
}

function setText(id, value) {
  const element = document.getElementById(id);
  if (element) element.textContent = value;
}

function asFiniteNumber(value) {
  if (value === null || value === undefined) {
    return null;
  }

  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : null;
}

function formatNumber(value, decimals = 0) {
  const numeric = asFiniteNumber(value);
  if (numeric === null) {
    return "--";
  }

  return numeric.toLocaleString(undefined, {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals
  });
}

function formatResistance(value) {
  const numeric = asFiniteNumber(value);
  if (numeric === null) {
    return "N/A";
  }

  return `${formatNumber(numeric, 1)} Ω`;
}

function formatRatio(value) {
  const numeric = asFiniteNumber(value);
  if (numeric === null) {
    return "N/A";
  }

  return formatNumber(numeric, 4);
}

function formatPpm(value) {
  const numeric = asFiniteNumber(value);
  if (numeric === null) {
    return "NOT CALIBRATED";
  }

  return `${formatNumber(numeric, 2)} ppm`;
}

function getSensorEntry(data, sensorKey) {
  const sensorList = Array.isArray(data.sensors) ? data.sensors : [];
  const existing = sensorList.find((sensor) => sensor.id === sensorKey) || {};

  const keySuffix = sensorKey === "mq3" ? "3" : sensorKey === "mq137" ? "137" : "138";
  const voltageKey = sensorKey === "mq3" ? "v3" : sensorKey === "mq137" ? "v137" : "v138";
  const rawKey = sensorKey === "mq3" ? "mq3" : sensorKey;
  const resistanceKey = `rs${keySuffix}`;
  const ratioKey = `ratio${keySuffix}`;
  const ppmKey = `ppm${keySuffix}`;

  return {
    id: sensorKey,
    targetGas: existing.targetGas ?? (
      sensorKey === "mq3"
        ? "ethanol"
        : sensorKey === "mq137"
          ? "ammonia (NH3)"
          : "VOC target gas required"
    ),
    units: existing.units ?? "ppm",
    raw: existing.raw ?? data[rawKey] ?? null,
    adcVoltage: existing.adcVoltage ?? data[voltageKey] ?? null,
    sensorResistance: existing.sensorResistance ?? data[resistanceKey] ?? null,
    rsRatio: existing.rsRatio ?? data[ratioKey] ?? null,
    ppm: existing.ppm ?? data[ppmKey] ?? null,
    calibrationStatus: existing.calibrationStatus ?? "UNCALIBRATED",
    threshold: existing.threshold ?? data[`t${keySuffix}`] ?? THRESHOLDS[sensorKey],
    healthy: existing.healthy ?? true,
    connected: existing.connected ?? true
  };
}

function renderGasSensor(sensorKey, sensorData) {
  const ui = SENSOR_UI[sensorKey];
  const rawValue = asFiniteNumber(sensorData.raw);
  const voltageValue = asFiniteNumber(sensorData.adcVoltage);
  const resistanceValue = asFiniteNumber(sensorData.sensorResistance);
  const ratioValue = asFiniteNumber(sensorData.rsRatio);
  const ppmValue = asFiniteNumber(sensorData.ppm);

  const rawIsValid = rawValue !== null;
  const voltageIsValid = voltageValue !== null;

  setText(ui.valueId, rawIsValid ? formatNumber(rawValue) : "---");
  setText(ui.voltageId, voltageIsValid ? `${formatNumber(voltageValue, 3)} V` : "-- V");
  setText(ui.resistanceId, resistanceValue === null ? "N/A" : formatResistance(resistanceValue));
  setText(ui.ratioId, ratioValue === null ? "N/A" : formatRatio(ratioValue));
  setText(ui.gasId, sensorData.targetGas || "UNSPECIFIED");
  setText(ui.ppmId, ppmValue === null ? "NOT CALIBRATED" : formatPpm(ppmValue));

  const progress = document.getElementById(ui.barId);
  if (progress) {
    progress.style.width = rawIsValid
      ? `${Math.min(100, Math.max(0, (rawValue / 4095) * 100))}%`
      : "0%";
  }

  const statusText = (sensorData.calibrationStatus || "NOT CALIBRATED").toUpperCase();
  const status = document.getElementById(ui.statusId);
  if (status) {
    status.textContent = statusText;
    status.classList.remove("status-uncalibrated");

    const isUncalibrated =
      statusText.includes("UNCALIBRATED") ||
      statusText.includes("CIRCUIT UNVERIFIED") ||
      statusText.includes("NOT CALIBRATED");

    if (isUncalibrated) {
      status.classList.add("status-uncalibrated");
    }

    status.style.color =
      statusText.includes("CALIBRATED") || statusText.includes("CIRCUIT VERIFIED") || statusText.includes("R0 CALIBRATED")
        ? "var(--green)"
        : isUncalibrated
          ? "#ff9b54"
          : "var(--red)";
  }

  return rawIsValid;
}

function renderDashboard(data) {
  const sensorData = {
    mq3: getSensorEntry(data, "mq3"),
    mq137: getSensorEntry(data, "mq137"),
    mq138: getSensorEntry(data, "mq138")
  };

  renderGasSensor("mq3", sensorData.mq3);
  renderGasSensor("mq137", sensorData.mq137);
  renderGasSensor("mq138", sensorData.mq138);

  const temperature = data.temp ?? data.temperature;
  const humidity = data.hum ?? data.humidity;
  setText("temperature", formatNumber(temperature, 1));
  setText("humidity", formatNumber(humidity, 1));

  const hasCalibratedReading = Object.values(sensorData).some(
    (sensor) => asFiniteNumber(sensor.ppm) !== null
  );

  systemStatus.textContent = hasCalibratedReading ? "CALIBRATION READY" : "PROTOTYPE MONITORING";
  systemStatus.style.color = hasCalibratedReading ? "var(--green)" : "var(--orange)";
  lastUpdated.textContent = `Last update: ${new Date().toLocaleTimeString()}`;
}

function generateDemoReadings() {
  demoReadings.mq3 = Math.max(0, Math.min(4095, 1640 + Math.round(Math.random() * 80 - 40)));
  demoReadings.mq137 = Math.max(0, Math.min(4095, 1320 + Math.round(Math.random() * 80 - 40)));
  demoReadings.mq138 = Math.max(0, Math.min(4095, 1710 + Math.round(Math.random() * 80 - 40)));

  demoReadings.v3 = demoReadings.mq3 * 3.3 / 4095;
  demoReadings.v137 = demoReadings.mq137 * 3.3 / 4095;
  demoReadings.v138 = demoReadings.mq138 * 3.3 / 4095;
  demoReadings.temp = 24 + Math.random() * 0.6 - 0.3;
  demoReadings.hum = 40 + Math.random() * 2 - 1;
  demoReadings.sensors = [
    {
      id: "mq3",
      targetGas: "ethanol",
      units: "ppm",
      raw: demoReadings.mq3,
      adcVoltage: demoReadings.v3,
      sensorResistance: null,
      rsRatio: null,
      ppm: null,
      calibrationStatus: "UNCALIBRATED",
      threshold: 1800,
      healthy: true,
      connected: true
    },
    {
      id: "mq137",
      targetGas: "ammonia (NH3)",
      units: "ppm",
      raw: demoReadings.mq137,
      adcVoltage: demoReadings.v137,
      sensorResistance: null,
      rsRatio: null,
      ppm: null,
      calibrationStatus: "UNCALIBRATED",
      threshold: 1800,
      healthy: true,
      connected: true
    },
    {
      id: "mq138",
      targetGas: "VOC target gas required",
      units: "ppm",
      raw: demoReadings.mq138,
      adcVoltage: demoReadings.v138,
      sensorResistance: null,
      rsRatio: null,
      ppm: null,
      calibrationStatus: "UNCALIBRATED",
      threshold: 1800,
      healthy: true,
      connected: true
    }
  ];

  return { ...demoReadings };
}

async function fetchSensorData() {
  if (DEMO_MODE) {
    setLocalDemoState();
    setDataSource("SIMULATED DATA");
    renderDashboard(generateDemoReadings());
    lastUpdated.textContent = `Simulated update: ${new Date().toLocaleTimeString()}`;
    return;
  }

  try {
    const response = await fetch(API_URL, {
      method: "GET",
      cache: "no-store"
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }

    const data = await response.json();

    if (
      typeof data.mq3 !== "number" ||
      typeof data.mq137 !== "number" ||
      typeof data.mq138 !== "number"
    ) {
      throw new Error("Unexpected sensor data format");
    }

    consecutiveFailures = 0;
    setConnectionState(true);
    setDataSource("LIVE ESP32 DATA");
    renderDashboard(data);
  } catch (error) {
    consecutiveFailures++;
    setConnectionState(false);
    setDataSource("LIVE ESP32 DATA");
    systemStatus.textContent = "DATA UNAVAILABLE";
    systemStatus.style.color = "var(--red)";
    lastUpdated.textContent = "Cannot reach ESP32 /api. Check the Wi-Fi connection.";
    console.warn("ESP32 API request failed:", error.message);
  }
}

fetchSensorData();
setInterval(fetchSensorData, REFRESH_INTERVAL_MS);
