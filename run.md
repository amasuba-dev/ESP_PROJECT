# ESP_PROJECT — Build & Flash Guide

## Prerequisites

- VS Code with the **Espressif IDF** extension (`espressif.esp-idf-extension`) installed
- Board connected via USB on **COM4** (CP210x driver installed)

---

## Step 1 — Open the ESP_PROJECT Workspace

In VS Code:

**File → Open Workspace from File…** → select:
```
C:\Users\u25737806\ESP_PROJECT\main\esp_project.code-workspace
```

This reloads VS Code with `ESP_PROJECT/` as the active workspace root (the folder containing the top-level `CMakeLists.txt`).

---

## Step 2 — First-Time ESP-IDF Setup (only once)

If you have never set up the Espressif extension on this machine:

1. Press **Ctrl+Shift+P** → type **"ESP-IDF: Configure ESP-IDF Extension"** → Enter
2. In the setup wizard that opens:
   - Select **"Download ESP-IDF"** and choose version **v5.5**  
     *(or "Use existing installation" if you already have it installed)*
   - Accept the default tool installation path (e.g. `C:\Users\<you>\.espressif`)
   - Click **"Install"** and wait for the download + extraction to complete (may take 10–20 minutes)
3. Once the wizard finishes you will see **"ESP-IDF is ready"** in the status bar.

> After setup, the extension provides its own managed terminal — you do **not** need to run `export.bat` manually.

---

## Step 3 — Set Target (first time per project)

Press **Ctrl+Shift+P** → **"ESP-IDF: Set Espressif Device Target"** → select **esp32s3** → select **ESP32-S3 chip (via ESP-USB-Bridge / USB-JTAG)**.

Or, open an **ESP-IDF Terminal** (bottom status bar → "ESP-IDF Terminal" button) and run:
```
idf.py set-target esp32s3
```

---

## Step 4 — Configure APN (first time)

Open **menuconfig** to set your SIM card's APN before building:

```
idf.py menuconfig
```

Navigate to: **ESP Project Configuration → SIM7670X 4G Modem Configuration → SIM card APN**

Set your carrier APN (e.g. `internet` for MTN, `web.vodacom.net` for Vodacom).

Press **S** to save, then **Q** to quit.

---

## Step 5 — Build

Press the **Build** button (hammer icon) in the VS Code status bar, or in the ESP-IDF terminal:

```
idf.py build
```

A successful build ends with:
```
[1/1] Generating binary image from built executables
esptool.py v4.x  Image ready.
```

---

## Step 6 — Flash + Monitor

Make sure the board is on **COM4**, then press the **Flash** and **Monitor** buttons in the status bar, or:

```
idf.py -p COM4 flash monitor
```

Press `Ctrl+]` to exit the serial monitor when done.

---

## VS Code Status Bar Quick Reference

| Button | Action |
|---|---|
| Hammer icon | Build project |
| Lightning icon | Flash to board |
| TV / plug icon | Open serial monitor |
| Gear icon | Open menuconfig |
| "ESP32-S3" chip label | Change target device |

---

## Rebuilding After Code Changes

No need to repeat `set-target` or `menuconfig`. Just:
```
idf.py build
idf.py -p COM4 flash monitor
```
Or combine (auto-builds before flash):
```
idf.py -p COM4 flash monitor
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Extension setup wizard never appears | Press **Ctrl+Shift+P** → "ESP-IDF: Configure ESP-IDF Extension" |
| `idf.py` not recognised | Use the ESP-IDF Terminal from the status bar, not plain PowerShell |
| `Access is denied` on COM4 | Another monitor session is open — press `Ctrl+]` first |
| Build fails after target change | Run `idf.py fullclean` then `idf.py build` |
| Modem not responding | SIM7670X needs a dedicated ≥2 A power supply, not USB 3.3 V |
| MPU-6050 not found | Check SDA=GPIO9, SCL=GPIO6 and 3.3 V on VCC (see `doc.md`) |
| Temperature always invalid | Check voltage divider: 10 kΩ series resistor + thermistor to GND on ADC1 CH0 |
| Current always 0 | Confirm SCT013 clamp jaw is around **one** conductor only; check shield A0 → ADC1 CH1 |
| Reed switch no pulses | GPIO10 pull-up is internal; connect switch between GPIO10 and GND |
