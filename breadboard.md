# Breadboard Wiring Guide — ESP_PROJECT Current Sensor

## Components on Breadboard

```
BREADBOARD LAYOUT (Standard 830-hole dual rail)

LEFT RAIL (Power)           RIGHT RAIL (Power)
  [+] 3.3V  ─────────────────  [+] 3.3V
  [-] GND   ─────────────────  [-] GND
   |                            |
   | (tie together via bottom)  |
```

---

## Component Placement & Connections

### **Power & Ground Distribution**

1. **3.3V Power Rail (left)**
   - Connect ESP32-S3A PIN_1 (3.3V) → breadboard **[+] column**
   - **Shield power taps from this rail**

2. **GND Power Rail (left)**
   - Connect ESP32-S3A PIN_2/37/38 (GND) → breadboard **[-] column**
   - **Shield ground taps from this rail**

---

## Current Sensor Wiring

### **Sensor: SCT013-030-1V via Energy Monitor Shield**

The Energy Monitor Shield V2 has a built-in bias circuit and terminal block for the SCT013. Plug the SCT013 jack directly into the shield's **CT1** socket (or A0 header), then wire the shield's A0 signal to the ESP32-S3A GPIO4 (PIN_7):

```
Energy Monitor Shield:
  • SCT013 3.5mm jack ──→ Shield CT1 socket (tip + sleeve)
  • Shield A0 pin ──→ ESP32-S3A GPIO4 (PIN_7)  [ADC input]

The shield provides the 100kΩ bias resistor and 10µF smoothing capacitor internally.
No additional components needed on breadboard for current sensor.
```

**Shield Pin Mapping (Arduino Uno layout):**
- **A0**: Current sensor ADC input (connects to ESP32-S3A GPIO4)
- **GND**: Common ground (tie to ESP32-S3A GND)
- **3.3V**: Power (tie to ESP32-S3A 3.3V if needed, but shield may have its own regulator)

Since your ESP32-S3A is not Arduino-compatible, **do not plug the shield directly onto the board**. Instead, place the shield on the breadboard and wire only the necessary signals (A0, GND, 3.3V) to the ESP32-S3A.

---

## Complete Breadboard Grid Map

```
     Left Rail    Breadboard Columns (A-J)           Right Rail

[+] 3.3V ────────────────────────────────────────── [+] 3.3V
 |   Row 1: Shield 3.3V ────────────────────────── [pin_1]
 |   Row 2: Shield GND ─────────────────────────── [pin_2]
 |   Row 3: Shield A0 ──────────────────────────── [pin_7]
 |   Row 4: (spare)
 |   Row 5: (spare)
 |   Row 6: (spare)
 |   Row 7: (spare)
 |   Row 8: (spare)
 |   Row 9: (spare)
 |   Row 10: (spare)
 |   Row 11: (spare)
 |   Row 12: (spare)
 |   Row 13: (spare)
 |   Row 14: (spare)
 |   Row 15: (spare)
 |   Row 16: (spare)
 |   Row 17: (spare)
 |   Row 18: (spare)
 |   Row 19: (spare)
 |   Row 20: (spare)
 |   Row 21: (spare)
 |   Row 22: (spare)
 |   Row 23: (spare)
 |   Row 24: (spare)
 |   Row 25: (spare)
 |   Row 26: (spare)
 |   Row 27: (spare)
 |   Row 28: (spare)
 |   Row 29: (spare)
 |   Row 30: (spare)
[-] GND ─────────────────────────────────────────── [-] GND
```

---

## Assembly Steps

1. Place ESP32-S3A on left side of breadboard
2. Place Energy Monitor Shield V2 on right side (headers up, not plugged on)
3. Connect jumper wires:
   - Shield A0 → ESP32-S3A PIN_7 (yellow wire)
   - Shield 3.3V → ESP32-S3A PIN_1 (red wire)
   - Shield GND → ESP32-S3A PIN_2 (black wire)
4. Plug SCT013-030-1V jack into shield CT1 socket
5. Clamp SCT013 jaw around one AC conductor to monitor
6. Power on via USB and test
 |   Row 9: reed ──────────────────────────────── [pin_25]
 |   Row 10: (spare)
 |   Row 11: (spare)
 |   Row 12: (spare)
 |   Row 13: (spare)
 |   Row 14: (spare)
 |   ...
[-] GND ───────────────────────────────────────── [-] GND
     ↑                                               ↑
  All GND connections                         (tie rails together)
  tap here
```

---

## Assembly Steps

1. **Power rails** (first)
   - Bridge 3.3V rail left ↔ right with a jumper at the bottom
   - Bridge GND rail left ↔ right with a jumper at the bottom

2. **Place resistors** (thermistor divider, current bias, optional I²C pull-ups)
   - 10kΩ from [+] to Row 1A (thermistor divider)
   - 100kΩ from [+] to Row 2A (current bias)

3. **Place capacitor**
   - 10µF from Row 2A to [-] GND (noise filter on current sensor)

4. **Place MPU-6050 module**
   - VCC → [+] rail, GND → [-] rail, AD0 → [-] rail
   - SDA/SCL jumpers to GPIO8/9

5. **Place reed switch** (2 wires)
   - One to Row 9, other to GND

6. **Place SIM7670X module** (built-in — no action needed)
   - SIM card: Insert nano-SIM into board slot
   - Power: Connect 4V LiPo to board's cellular input

7. **Plug in thermistor & SCT013**
   - Thermistor legs into Row 1A and GND
   - SCT013 jack tip to Row 2A, sleeve to GND

8. **Connect ESP32-S3A jumpers**
   - PIN_1 (3.3V) → [+] rail
   - PIN_2 (GND) → [-] rail

   - PIN_21 (GPIO8 SDA) → Row 3B
   - PIN_23 (GPIO9 SCL) → Row 4B
   - PIN_25 (GPIO10) → Row 9A

---

## Recommended Efficient Layout

Place components in this order to minimize wire crossings and keep the breadboard organized:

### **Zone A: Power Distribution (Rows 1–3, Left Side)**

```
  A   B   C   D   E  ...
1 [+] R1  Ther|mist|or
   3V  10k  GP|IO1 |
2 [+] R2  SCT |013 |
   3V  100k ADC|out|
3 [+] C1  CAP |to  |
   3V  10µF GN|D   |
```

- **Column A**: all [+] 3.3V rail connections
- **Column B**: resistors R1 (10kΩ), R2 (100kΩ), capacitor C1
- **Column C+**: sensor inputs (thermistor, SCT013 jack tip)
- **Row connectors**: Jumpers to GPIO1, GPIO4

---

### **Zone B: I²C Sensors (Rows 4–8, Center)**

```
  F   G   H   I   J
4 MPU SDA       TO GPIO8
5 6050 SCL      TO GPIO9
6 VCC [+]
7 GND [-]
8 AD0 [-]
```

- **Column F**: MPU-6050 module pins labeled vertically
- **Columns G–I**: internal module connections
- **Column J**: jumpers to ESP32-S3A (GPIO8, GPIO9, 3.3V, GND)

Keep the MPU module compact in its own zone so I²C wires don't cross other sensors.

---

### **Zone C: Discrete Sensors (Rows 9–12, Right Side)**

```
  K   L   M   N
9 REED    TO GPIO10
10      [-] GND
11 TXD MODEM  GPIO44
12 RXD       GPIO43
```

- **Row 9–10**: Reed switch (2 wires: one to GPIO10, one to GND)
- **Rows 11–14**: SIM7670X module (4 wires: TXD/RXD to GPIOs, GND, VCC to separate supply)

---

## Wiring Sequence (Fastest Assembly)

1. **Lay down power rails** (3.3V and GND jumpers connecting left ↔ right)

2. **Zone A first** (all on left side, no crossing):
   - Resistor R1 (10kΩ) from [+] → Row 1
   - Resistor R2 (100kΩ) from [+] → Row 2
   - Capacitor C1 from Row 2 → [-]
   - Thermistor legs into Row 1 and [-]
   - SCT013 jack into Row 2 and [-]
   - 2 jumpers: Row 1→GPIO1, Row 2→GPIO4

3. **Zone B** (center, isolated):
   - Drop MPU-6050 module into Rows 4–8
   - 4 jumpers: VCC→[+], GND→[-], AD0→[-], SDA→GPIO8, SCL→GPIO9

4. **Zone C** (right side, clean):
   - Reed switch into Row 9 and GND
   - 1 jumper: Row 9 → GPIO10
   - SIM7670X: Built-in, no breadboard wiring needed

**Total jumpers to ESP32-S3A: only 7** (minimal clutter, easy to trace)

---

## Breadboard Material List

- **Resistors:** 10kΩ (1×), 100kΩ (1×), 4.7kΩ (2× optional)
- **Capacitor:** 10µF ceramic (1×)
- **Jumper wires:** ~20–25 pieces (mix of colors)
- **830-hole breadboard:** (1×)

All connections are **non-permanent**, so mistakes can be corrected by pushing wires to new holes.
