# ESP_PROJECT

Embedded firmware for the **ESP32-S3A / SIM7670X-4G** sensor node.  
Reads electrical current parameters, transmits JSON payloads to a cloud portal via 4G, and observes a configurable day/night schedule aligned to **South African Standard Time (SAST, UTC+2)**.

---

## Hardware

| Component | Model | Interface |
|-----------|-------|-----------|
| Microcontroller | ESP32-S3A (QFN56, rev 0.2) | — |
| 4G Modem | SIM7670X | UART (AT commands) |
| Current Sensor | SCT013 30A/1V, Class 1 (YHDC) | ADC (Energy Monitor Shield A0) |
| Energy Monitor Shield | v2 (devicter.ru / Seeed 106990026) | ADC |

### Energy Monitor Shield V2 — Pin Mapping

| Shield Pin | Function | ESP32-S3A GPIO |
|------------|----------|----------------|
| A0 | Current sensor (SCT013) | GPIO4 (PIN_7) |
| GND | Common ground | GND (PIN_2) |
| 3.3V | Power | 3.3V (PIN_1) |

> The shield provides the bias circuit for the SCT013 current clamp.  
> Wire A0 to ESP32-S3A GPIO4 (PIN_7) for ADC input.

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
    │   ├── current.c/h          # SCT013 30A/1V → Irms, power, energy
    ├── modem/
    │   ├── sim7670x.c/h         # UART AT driver + HTTP POST
    └── portal/
        ├── portal.c/h           # JSON serialization + event field
```

---

## SAST Sleep Schedule

The device operates on a **day/night cycle aligned to SAST (UTC+2)**:

- **Daytime (06:00–18:00 SAST)**: Continuous measurement every 10 seconds
- **Nighttime (18:00–06:00 SAST)**: Deep sleep with timer wakeup

Network time is synced via SIM7670X AT+CCLK. If time unavailable, operates continuously.

---

## JSON Payload Schema

```json
{
  "device": "ESP32-S3-001",
  "timestamp_ms": 1700000000000,
  "event": "data|wake|sleep",
  "current": {
    "irms_a": 1.234,
    "power_w": 271.0,
    "energy_kwh": 0.0001
  },
  "signal_rssi_dbm": -75
}
```

- **`event`**: `"data"` (normal measurement), `"wake"` (startup), `"sleep"` (shutdown)
- **`current`**: SCT013 readings (RMS current, apparent power, accumulated energy)
- **`signal_rssi_dbm`**: 4G signal strength

---

## Build & Flash

```bash
# Configure pins (optional — defaults match hardware)
idf.py menuconfig

# Build
idf.py build

# Flash to ESP32-S3 on COM4
idf.py -p COM4 flash

# Monitor serial output
idf.py -p COM4 monitor
```

---

## menuconfig Options

| Path | Key | Default | Description |
|------|-----|---------|-------------|
| ESP Project Configuration → Sensor Pin Configuration | `CURRENT_ADC_CHANNEL` | 4 | ADC1 CH4 = GPIO4 (PIN_7) |
| ESP Project Configuration → Device Settings | `DEVICE_ID` | "ESP32-S3-001" | Unique device name |
| ESP Project Configuration → Portal Settings | `PORTAL_URL` | "http://portal.example.com/api/data" | HTTP POST endpoint |
| ESP Project Configuration → Measurement Settings | `MEASURE_INTERVAL_MS` | 10000 | Sensor read interval (ms) |
| ESP Project Configuration → Electrical Calibration | `SUPPLY_VOLTAGE` | 220 | Mains voltage (V) for power calc |

---

## Portal Integration

The device sends HTTP POST requests with JSON payloads. Implement a server endpoint that:

1. Accepts `application/json` POSTs
2. Parses the schema above
3. Stores readings in a database
4. Returns 200 OK on success

Example server code (Node.js/Express):

```javascript
app.post('/api/data', (req, res) => {
  const data = req.body;
  console.log(`Device ${data.device}: ${data.current.irms_a}A`);
  // Store in DB...
  res.sendStatus(200);
});
```

---

## Calibration

### Current Sensor (SCT013-030-1V)

1. Clamp around **one conductor only** (live or neutral)
2. Apply known resistive load (e.g., 1000W heater)
3. Measure actual current with multimeter
4. Adjust `CURRENT_CAL` in `current.c`:
   ```
   #define CURRENT_CAL  30.0f  // Tune this value
   ```
5. Rebuild and reflash

### Energy Accumulation

Energy (kWh) accumulates across deep sleep cycles using NVS flash storage.

---

## Troubleshooting

- **No current readings**: Check SCT013 jack in CT1 socket, clamp orientation
- **Portal connection fails**: Verify SIM card, APN settings in `sim7670x.c`
- **Time sync fails**: Check SIM7670X network registration
- **Deep sleep not working**: Confirm GPIO4 not conflicting with other peripherals

---

## License

MIT License — see LICENSE file.

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
