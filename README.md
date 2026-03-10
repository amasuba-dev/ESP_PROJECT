# ESP_PROJECT

Embedded firmware for the **ESP32-S3A / SIM7670X-4G** sensor node.  
Reads environmental and electrical parameters, transmits JSON payloads to a cloud portal via 4G, and observes a configurable day/night schedule aligned to **South African Standard Time (SAST, UTC+2)**.

---

## Hardware

| Component | Model | Interface |
|-----------|-------|-----------|
| Microcontroller | ESP32-S3A (QFN56, rev 0.2) | — |
| 4G Modem | SIM7670X | UART (AT commands) |
| Temperature Sensor | HFT 105.5/2.7 NTC Thermistor | ADC (voltage divider) |
| Current Sensor | SCT013 30A/1V, Class 1 (YHDC) | ADC (Energy Monitor Shield A0) |
| Accelerometer / Gyro | MPU-6050 SY-104 | I2C |
| Reed Switch | Magnetic door/window sensor | GPIO (interrupt) |
| Energy Monitor Shield | v2 (devicter.ru / Seeed 106990026) | ADC · I2C · RS485 |

### Energy Monitor Shield V2 — Pin Mapping

| Shield Pin | Function | ESP32-S3A GPIO |
|------------|----------|----------------|
| A0 | Current sensor (SCT013) | Configurable via `menuconfig` |
| A1 | Temperature sensor (HFT 105.5/2.7) | Configurable via `menuconfig` |
| A4 / SDA | I2C Data | Configurable via `menuconfig` |
| A5 / SCL | I2C Clock | Configurable via `menuconfig` |
| D0 / RX | RS485 MAX485 RO | Configurable via `menuconfig` |
| D1 / TX | RS485 MAX485 DI | Configurable via `menuconfig` |
| D5 | RS485 Direction (DE/RE) | Configurable via `menuconfig` |

> Exact GPIO assignments depend on how the shield is wired to the ESP32-S3A breakout.  
> Set all pins via **`idf.py menuconfig` → ESP Project Configuration**.

---

## Project Structure

```
ESP_PROJECT/
├── CMakeLists.txt
├── sdkconfig.defaults          # esp32s3 defaults
├── .gitignore
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── Kconfig.projbuild        # All runtime config (pins, URL, intervals)
    ├── app_main.c               # Scheduler + main measurement loop
    ├── sensors/
    │   ├── temperature.c/h      # HFT 105.5/2.7 NTC → °C (Beta equation)
    │   ├── current.c/h          # SCT013 RMS current, power, energy
    │   ├── mpu6050.c/h          # I2C accelerometer + gyroscope
    │   └── reed_switch.c/h      # GPIO edge interrupt with debounce
    ├── modem/
    │   └── sim7670x.c/h         # SIM7670X AT command driver + HTTP POST
    └── portal/
        └── portal.c/h           # JSON serialiser + cloud send
```

---

## Day / Night Schedule (SAST = UTC+2)

The device operates on a **24-hour schedule** governed by network time from the SIM7670X modem (`AT+CCLK?`):

| Period | SAST | UTC | Action |
|--------|------|-----|--------|
| **Wake-up** | 06:00 | 04:00 | Send status update → begin monitoring |
| **Day mode** | 06:00–18:00 | 04:00–16:00 | Sample all sensors every `MEASURE_INTERVAL_MS`, POST to portal |
| **Sleep transition** | 18:00 | 16:00 | Send final status update → enter ESP32 deep sleep |
| **Sleep** | 18:00–06:00 | 16:00–04:00 | Deep sleep (12 h), modem powered off |

### Wake-Up & Sleep Messages (sent to portal)

**Wake-up payload** includes all sensor readings plus `"event": "wake"`.  
**Sleep payload** includes all sensor readings plus `"event": "sleep"` and `"next_wake_utc"`.

### How Network Time Is Obtained

1. After boot, `sim7670x_sync_time()` issues `AT+CCLK?` to the modem.
2. The modem returns current UTC time from the cellular network.
3. The SAST offset (+2 h) is applied in firmware — no external NTP needed.
4. If the modem cannot provide time within 60 s, the device falls back to measurement-only mode (no sleep scheduling) and logs a warning.

---

## Portal / Cloud

Sensor data is transmitted as JSON via **HTTP POST** over 4G to the configured endpoint.

### JSON Schema

```json
{
  "device_id": "ESP32-S3-001",
  "timestamp": 1234567890123,
  "event": "data",
  "temperature": { "value": 24.5, "valid": true },
  "current": {
    "irms": 1.234,
    "power": 271.5,
    "energy": 0.0075,
    "valid": true
  },
  "accelerometer": {
    "ax": 0.12, "ay": -0.05, "az": 9.78,
    "gx": 0.1,  "gy": 0.0,  "gz": -0.2,
    "valid": true
  },
  "reed": { "pulses": 42, "state": true },
  "rssi_dbm": -71
}
```

`"event"` values: `"data"` · `"wake"` · `"sleep"`

---

## Building & Flashing

### Prerequisites

- ESP-IDF v5.5.3 installed at `C:\Espressif\frameworks\esp-idf-v5.5.3`
- Open a terminal that has sourced `export.bat` (the workspace profile does this automatically)

### First-time setup

```cmd
cd C:\Users\user\Documents\ESP_PROJECT
idf.py set-target esp32s3
idf.py menuconfig
```

Key settings to configure in `menuconfig`:

- **ESP Project Configuration → Sensor Pin Configuration** — ADC channels and GPIO numbers
- **ESP Project Configuration → SIM7670X 4G Modem Configuration** — UART port and GPIOs
- **ESP Project Configuration → Portal / Cloud Configuration** — portal URL and device ID
- **ESP Project Configuration → Electrical Calibration** — local mains voltage (default 220 V)

### Build and flash

```cmd
idf.py build
idf.py -p COM4 flash monitor
```

---

## Sensor Calibration

### HFT 105.5/2.7 Temperature Sensor

Update the following constants in [main/sensors/temperature.c](main/sensors/temperature.c) once the sensor datasheet is confirmed:

```c
#define TEMP_BETA       3950.0f   // Beta coefficient (K) — check datasheet
#define TEMP_NOMINAL_R  10000.0f  // Resistance at 25°C (Ω)
#define SERIES_R        10000.0f  // Series resistor in voltage divider (Ω)
```

### SCT013 30A/1V Current Sensor

The calibration constant in [main/sensors/current.c](main/sensors/current.c):

```c
#define CURRENT_CAL  30.0f  // A/V (30 A ÷ 1 V — factory value for SCT013-030-1V)
```

Verify against a known resistive load and adjust if readings drift.

---

## Repository

**GitHub:** https://github.com/amasuba-dev/ESP_PROJECT

---

## License

This project is released under the MIT License.
