# ESP_PROJECT — Build & Flash Guide

## Prerequisites

- VS Code with the **ESP-IDF extension** installed
- ESP-IDF v5.5.3 at `C:\Espressif\frameworks\esp-idf-v5.5.3`
- Board connected via USB on **COM4**

---

## Step 1 — Open the ESP_PROJECT Workspace

In VS Code:

**File → Open Workspace from File…** → select:
```
C:\Users\user\Documents\ESP_PROJECT\esp_project.code-workspace
```

This reloads VS Code with ESP_PROJECT as the active workspace and automatically opens an ESP-IDF terminal (CMD with `export.bat` already sourced).

---

## Step 2 — Set Target (first time only)

In the ESP-IDF terminal that opens, run:
```
idf.py set-target esp32s3
```

---

## Step 3 — Build

```
idf.py build
```

A successful build ends with:
```
[1/1] Generating binary image from built executables
esptool.py v4.x  Image ready.
```

---

## Step 4 — Flash + Monitor

Make sure the board is on **COM4**, then:
```
idf.py -p COM4 flash monitor
```

Press `Ctrl+]` to exit the serial monitor when done.

---

## Alternative: VS Code ESP-IDF Sidebar

Once the ESP_PROJECT workspace is open, use the ESP-IDF extension buttons in the VS Code status bar (bottom):

| Button | Action |
|---|---|
| Hammer icon | Build project |
| Lightning icon | Flash to board |
| Plug icon | Open serial monitor |

---

## Rebuilding After Code Changes

No need to repeat `set-target`. Just run:
```
idf.py build
idf.py -p COM4 flash monitor
```

Or combine into one command:
```
idf.py -p COM4 flash monitor
```
(ESP-IDF will rebuild automatically before flashing if source files have changed.)

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `idf.py` not recognized | Make sure you are in the ESP-IDF terminal, not a plain PowerShell terminal |
| `Access is denied` on COM4 | Another monitor session is already open — press `Ctrl+]` to close it first |
| Build fails after target change | Run `idf.py fullclean` then `idf.py build` |
| Modem not responding | Check SIM7670X power supply (needs dedicated ≥2 A source, not USB 3.3V) |
