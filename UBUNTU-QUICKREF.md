# Ubuntu Quick Reference — ESP_PROJECT Commands

## Every Terminal Session

```bash
# Activate ESP-IDF environment
source /home/titan/aaron/testcv/ESP_PROJECT/esp/esp-idf/export.sh

# Navigate to project (optional - keep in mind for builds)
cd /home/titan/aaron/testcv/ESP_PROJECT
```

## Build & Flash (Standard Workflow)

```bash
# One-liner: builds, flashes, and opens monitor
idf.py -p /dev/ttyACM0 flash monitor

# Or step-by-step:
idf.py build                       # Build only
idf.py -p /dev/ttyACM0 flash       # Flash only
idf.py -p /dev/ttyACM0 monitor     # Monitor only
```

## First-Time Setup

```bash
# Install everything
bash /home/titan/aaron/testcv/ESP_PROJECT/esp-setup.sh

# Configure APN
idf.py menuconfig
# Navigate to: ESP Project Configuration → SIM7670X → SIM card APN
# Save (S) and Quit (Q)
```

## Using Helper Script

```bash
cd /home/titan/aaron/testcv/ESP_PROJECT
./build-and-flash.sh /dev/ttyACM0
```

## Find USB Port

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

## Common Issues & Fixes

| Problem | Solution |
|---------|----------|
| "Port not found" | Check `ls /dev/ttyUSB*` and use actual port |
| Permission denied | `sudo usermod -a -G dialout $USER` then logout |
| "ESP-IDF not found" | Run `source ~/esp/esp-idf/export.sh` |
| Build errors | Run `idf.py fullclean && idf.py build` |

## Monitor Commands

- Exit monitor: **Ctrl+]**
- Clear screen: **Ctrl+L**
- Send break: **Ctrl+T x**

## Useful Flags

```bash
# Verbose output (see all compile details)
idf.py -v build

# Specific baud rate for monitoring
idf.py -p /dev/ttyACM0 -b 115200 monitor

# Set log level
idf.py build -DLOG_VERBOSITY=<NONE|ERROR|WARN|INFO|DEBUG|VERBOSE>
```

## Environment Persistence

Add to `~/.bashrc` for automatic activation:

```bash
# Add this line to ~/.bashrc
alias idf-activate='source ~/esp/esp-idf/export.sh'

# Then reload
source ~/.bashrc

# Use with:
idf-activate
```

---

See [ESP_PROJECT-ubuntu-setup.md](ESP_PROJECT-ubuntu-setup.md) for detailed setup guide.
