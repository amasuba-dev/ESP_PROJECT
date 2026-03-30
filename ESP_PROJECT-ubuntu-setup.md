# ESP_PROJECT — Ubuntu Setup & Build Guide

## Quick Start (5 minutes)

### 1. Install ESP-IDF (first time only)
```bash
cd /home/titan/aaron/testcv/ESP_PROJECT
bash esp-setup.sh
```

### 2. Source the environment each terminal session
```bash
source /home/titan/aaron/testcv/ESP_PROJECT/esp/esp-idf/export.sh
```

Or add this alias to `~/.bashrc` for convenience:
```bash
alias idf-activate='source ~/esp/esp-idf/export.sh'
```

Then you can simply run:
```bash
idf-activate
```

### 3. Build the project
```bash
cd /home/titan/aaron/testcv/ESP_PROJECT
idf.py build
```

### 4. Find your USB port
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# Common ports: /dev/ttyUSB0, /dev/ttyACM0
```

### 5. Flash to the board
```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Replace `/dev/ttyUSB0` with your actual port. This will:
- Build (if needed)
- Flash the firmware
- Open the serial monitor (Ctrl+] to exit)

---

## Alternative: Use helper script

```bash
# Build and flash
./build-and-flash.sh /dev/ttyACM0

# Just build
idf.py build

# Just flash
idf.py -p /dev/ttyACM0 flash

# Just monitor
idf.py -p /dev/ttyACM0 monitor
```

---

## Configuration (menuconfig)

Before first flash, configure the project:

```bash
source ~/esp/esp-idf/export.sh
cd /home/titan/aaron/testcv/ESP_PROJECT
idf.py menuconfig
```

Navigate to: **ESP Project Configuration → SIM7670X 4G Modem Configuration → SIM card APN**

Set your carrier APN (e.g., `internet`, `web.vodacom.net`).

Then: **S** to save, **Q** to quit.

---

## Hardware Connection

### USB Connection
- Connect ESP32-S3 board via USB cable
- The port will appear as `/dev/ttyUSB0`, `/dev/ttyACM0`, or similar
- Check permissions: `sudo usermod -a -G dialout $USER` (then logout/login)

### Sensor Connections
See [doc.md](doc.md) and [breadboard.md](breadboard.md) for wiring details:
- **Current sensor (SCT013-030-1V)** → Energy Monitor Shield A0 → GPIO4
- **Temperature sensor (HFT 105.5)** → Shield A1 → GPIO1
- **MPU-6050 accelerometer** → I²C (SDA GPIO8, SCL GPIO9)
- **Reed switch** → GPIO10
- **SIM7670X modem** → UART2 (TX GPIO43, RX GPIO44)

---

## Project Structure

```
ESP_PROJECT/
├── ESP_PROJECT-ubuntu-setup.md  (this file)
├── esp-setup.sh                  (automatic setup script)
├── build-and-flash.sh            (build & flash helper)
├── CMakeLists.txt
├── sdkconfig.defaults            (default configurations)
├── main/
│   ├── app_main.c                (main application)
│   ├── CMakeLists.txt
│   ├── sensors/
│   │   ├── temperature.c/h
│   │   ├── current.c/h
│   │   ├── mpu6050.c/h
│   │   └── reed_switch.c/h
│   ├── modem/
│   │   └── sim7670x.c/h
│   └── portal/
│       └── portal.c/h
└── build/                        (generated - firmware binaries)
```

---

## Build Output

After building, firmware files are in `build/`:
- `esp_project.bin` — Main firmware binary
- `bootloader/bootloader.bin` — Bootloader
- `partition_table/partition-table.bin` — Partition table

---

## Troubleshooting

### Port not found
```bash
# List all USB devices
lsusb

# Check if permissions needed
sudo usermod -a -G dialout $USER
# Then logout and login
```

### "ESP-IDF not found"
```bash
source ~/esp/esp-iDF/export.sh
```

### Build errors
```bash
# Full rebuild
idf.py fullclean
idf.py build
```

### Permission denied for /dev/ttyUSB0
```bash
sudo usermod -a -G dialout $USER
newgrp dialout
# Or logout and login
```

---

## Serial Monitor Commands

While in serial monitor (started with `idf.py monitor`):
- **Ctrl+]** — Exit monitor
- **Ctrl+T x** — Create a break (x = signal type)
- **Ctrl+L** — Clear screen

View logs:
```bash
idf.py -p /dev/ttyUSB0 monitor --baud 115200
```

---

## Development Workflow

1. **Edit code** in VSCode
2. **Source environment**: `source ~/esp/esp-idf/export.sh`
3. **Build**: `idf.py build`
4. **Flash**: `idf.py -p /dev/ttyUSB0 flash`
5. **Monitor**: `idf.py -p /dev/ttyUSB0 monitor`
6. **One-liner**: `idf.py -p /dev/ttyUSB0 flash monitor`

---

## Additional Resources

- [ESP-IDF Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [SIM7670X AT Command Reference](https://www.simcom.com/en/)

---

## Notes

- **APN Configuration**: Must be set in menuconfig before first build for modem connectivity
- **Device ID**: Configurable via menuconfig (default: "ESP32-S3-001")
- **API Endpoint**: Set in menuconfig under Portal Configuration
- **Time Sync**: Device syncs time from modem (SAST UTC+2 for day/night schedule)

---

Created for Ubuntu environment. Last updated: March 30, 2026
