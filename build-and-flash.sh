#!/bin/bash
# ESP_PROJECT Build & Flash Helper
# Usage: ./build-and-flash.sh [port]
# Example: ./build-and-flash.sh /dev/ttyACM0

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Default port for Waveshare ESP32-S3-A-SIM7670X-4G HAT
PORT=${1:-/dev/ttyACM0}

# Ensure IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo -e "${BLUE}Sourcing ESP-IDF environment...${NC}"
    if [ -f "$(dirname "$0")/esp/esp-idf/export.sh" ]; then
        source "$(dirname "$0")/esp/esp-idf/export.sh"
    else
        source ~/esp/esp-idf/export.sh
    fi
fi

cd "$(dirname "$0")"

# Check port
if [ ! -e "$PORT" ]; then
    echo -e "${RED}✗ Port $PORT not found${NC}"
    echo "Available ports:"
    ls -1 /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "No USB ports found"
    exit 1
fi

echo -e "${BLUE}=== ESP_PROJECT Build & Flash ===${NC}"
echo "Port: $PORT"

# Build
echo -e "${BLUE}Building...${NC}"
idf.py build
echo -e "${GREEN}✓ Build complete${NC}"

# Flash
echo -e "${BLUE}Flashing to $PORT...${NC}"
idf.py -p "$PORT" flash
echo -e "${GREEN}✓ Flash complete${NC}"

# Monitor
echo -e "${BLUE}Opening serial monitor (Ctrl+] to exit)...${NC}"
idf.py -p "$PORT" monitor
