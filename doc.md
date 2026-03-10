# ESP_PROJECT — Hardware Connection Guide

## Overview

```
[ ESP32-S3A dev board ]
        ↕  (Arduino header)
[ Energy Monitor Shield V2 ]
        ↕  separate wires
[ SIM7670X-4G modem module ]
```

The Energy Monitor Shield plugs directly onto the ESP32-S3A via standard Arduino-compatible headers.  
The SIM7670X connects via UART wires to dedicated GPIOs.

---

## 1. Energy Monitor Shield V2 → ESP32-S3A Header Mapping

| Shield label | Arduino pin | ESP32-S3A GPIO | Signal |
|---|---|---|---|
| A0 | Analog 0 | **GPIO1** (ADC1 CH0) | SCT013 current output |
| A1 | Analog 1 | **GPIO2** (ADC1 CH1) | HFT thermistor output |
| A4 | Analog 4 | **GPIO8** | I²C SDA (MPU-6050) |
| A5 | Analog 5 | **GPIO9** | I²C SCL (MPU-6050) |
| D0 | Digital 0 | **GPIO18** | RS485 RO (MAX485 → ESP RX) |
| D1 | Digital 1 | **GPIO17** | RS485 DI (ESP TX → MAX485) |
| D5 | Digital 5 | **GPIO15** | RS485 DE/RE direction |
| 3.3V | 3.3V rail | 3.3V | Shield power |
| GND | GND | GND | Common ground |

> If your ESP32-S3A board does not have a standard Arduino header layout, verify which physical pin corresponds to each GPIO number before soldering.

---

## 2. SCT013-030-1V Current Clamp → Shield A0

The **SCT013-030-1V** variant has a built-in burden resistor and outputs 0–1 V AC.

```
SCT013 3.5mm jack (tip + sleeve) ──→ Shield terminal block "CT1" (or A0 header)
```

- The shield's bias circuit centres the AC signal at VCC/2 to fit the 0–3.3 V ADC range.
- **Do not add an external burden resistor** — the 1V model already includes one.
- Clamp the jaw around **one conductor only** (live or neutral — not both).
- Reversing the clamp orientation reverses the current sign.

---

## 3. HFT 105.5/2.7 Thermistor → Shield A1

Wire as a voltage divider with a 10 kΩ series resistor:

```
3.3V ──[ 10kΩ ]──┬── Shield A1 (→ GPIO2 ADC)
                 └── Thermistor ── GND
```

> Check the shield silk screen — if pads labelled "NTC" or "R_S" are present on A1, solder the thermistor and series resistor there directly instead of point-to-point wiring.

---

## 4. MPU-6050 Accelerometer → Shield I²C (A4 / A5)

| MPU-6050 pin | Connects to |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | Shield A4 → GPIO8 |
| SCL | Shield A5 → GPIO9 |
| AD0 | GND (sets I²C address **0x68**) |
| INT | Not connected (polling mode) |

Pull-ups: internal pull-ups are enabled in firmware. If the I²C bus is long or noisy, add 4.7 kΩ external pull-ups to 3.3V on SDA and SCL.

---

## 5. Reed Switch → GPIO10

```
Reed switch
  Pin 1 ── GPIO10
  Pin 2 ── GND
```

GPIO10 is configured as input with internal pull-up and edge-triggered interrupt. No external resistor required.

---

## 6. SIM7670X 4G Modem → ESP32-S3A (UART2)

| SIM7670X pin | ESP32-S3A GPIO | Direction |
|---|---|---|
| TXD | **GPIO44** | Modem → ESP (ESP RX) |
| RXD | **GPIO43** | ESP → Modem (ESP TX) |
| GND | GND | Common |
| VCC / VBAT | **4V dedicated supply** | See note below |

> **Critical:** The SIM7670X draws up to 2 A during 4G transmission. **Do not power it from the ESP32-S3A 3.3V pin.** Use a dedicated LiPo battery or a 4V regulator rated ≥ 2 A. Connect GNDs together.

Insert a nano-SIM with mobile data enabled and no PIN lock.

---

## 7. RS485 / MAX485 — Optional

Used if RS485 sensors are connected to the bus.

| MAX485 pin | ESP32-S3A GPIO | Shield label |
|---|---|---|
| DI (driver input) | GPIO17 | D1 |
| RO (receiver output) | GPIO18 | D0 |
| DE + RE (tied together) | GPIO15 | D5 |
| VCC | 5V or 3.3V (check MAX485 datasheet) | Shield VCC |
| GND | GND | GND |

DE/RE is driven HIGH to transmit and LOW to receive. Connect RS485 A/B terminals to the sensor bus.

---

## 8. Power Summary

| Component | Supply | Max current |
|---|---|---|
| ESP32-S3A | 5V via USB or 3.3V LDO | ~500 mA |
| Energy Monitor Shield | 3.3V from ESP32-S3A | ~50 mA |
| MPU-6050 | 3.3V from shield | ~4 mA |
| HFT thermistor + divider | 3.3V from shield | ~0.5 mA |
| Reed switch | 3.3V pull-up (internal) | negligible |
| SIM7670X | **Dedicated LiPo / 4V ≥ 2A** | up to 2 A |

---

## 9. menuconfig Pin Verification

Before building, confirm all GPIO/channel numbers match your physical wiring:

```
idf.py menuconfig
```

Navigate to **ESP Project Configuration → Sensor Pin Configuration**.  
All default values match the table above. Adjust if your board maps Arduino headers differently.

| menuconfig key | Default value | Notes |
|---|---|---|
| `TEMP_ADC_CHANNEL` | 0 | ADC1 CH0 = GPIO1 |
| `CURRENT_ADC_CHANNEL` | 1 | ADC1 CH1 = GPIO2 |
| `I2C_SDA_GPIO` | 8 | |
| `I2C_SCL_GPIO` | 9 | |
| `REED_SWITCH_GPIO` | 10 | |
| `RS485_TX_GPIO` | 17 | |
| `RS485_RX_GPIO` | 18 | |
| `RS485_DE_GPIO` | 15 | |
| `SIM7670X_TX_GPIO` | 43 | ESP TX → Modem RX |
| `SIM7670X_RX_GPIO` | 44 | Modem TX → ESP RX |
