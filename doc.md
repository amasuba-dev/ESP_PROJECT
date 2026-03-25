# ESP_PROJECT — Hardware Connection Guide

## Overview

```
[ ESP32-S3A dev board ]
        ↕  separate wires
[ Energy Monitor Shield V2 ]
        ↕  SCT013 current clamp
```

The Energy Monitor Shield V2 provides the bias circuit for the SCT013 current sensor. Wire the shield's A0 output to the ESP32-S3A GPIO4 (PIN_7).

---

## 1. Energy Monitor Shield V2 → ESP32-S3A Wiring

| Shield label | Arduino pin | ESP32-S3A GPIO | ESP32-S3A PIN | Signal |
|---|---|---|---|---|
| A0 | Analog 0 | **GPIO4** (ADC1 CH4) | **PIN_7** | SCT013 current output |
| 3.3V | 3.3V rail | 3.3V | **PIN_1** | Shield power |
| GND | GND | GND | **PIN_2** | Common ground |

> Your Waveshare ESP32-S3A board is not Arduino-compatible, so the shield cannot plug directly on. Wire point-to-point using the PIN numbers above.

---

## 2. SCT013-030-1V Current Clamp → Energy Monitor Shield A0

The **SCT013-030-1V** variant has a built-in burden resistor and outputs 0–1 V AC.

Plug the SCT013 3.5mm jack directly into the Energy Monitor Shield's **CT1** socket (or A0 header). The shield provides the bias circuit internally.

Then, wire the shield's **A0 pin** to ESP32-S3A **GPIO4 (PIN_7)**:

```
Shield A0 ──→ ESP32-S3A GPIO4 (PIN_7)
```

- The shield's bias circuit centres the AC signal at VCC/2 to fit the 0–3.3 V ADC range.
- **Do not add an external burden resistor** — the 1V model already includes one.
- Clamp the jaw around **one conductor only** (live or neutral — not both).
- Reversing the clamp orientation reverses the current sign.

**Shield Power & Ground:**
- Connect shield GND to ESP32-S3A GND (PIN_2)
- Connect shield 3.3V to ESP32-S3A 3.3V (PIN_1) if the shield needs power (it may have its own regulator)

---

## 3. Power Summary

| Component | Supply | Max current |
|---|---|---|
| ESP32-S3A | 5V via USB or 3.3V LDO | ~500 mA |
| Energy Monitor Shield | 3.3V from ESP32-S3A | ~50 mA |
| SCT013 current clamp | Powered via shield | negligible |

---

## 4. menuconfig Pin Verification

Before building, confirm the GPIO/channel number matches your wiring:

```
idf.py menuconfig
```

Navigate to **ESP Project Configuration → Sensor Pin Configuration**.

| menuconfig key | Default value | Notes |
|---|---|---|
| `CURRENT_ADC_CHANNEL` | 4 | ADC1 CH4 = GPIO4 (PIN_7) |
