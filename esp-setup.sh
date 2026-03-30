#!/bin/bash
# ESP-IDF Setup Script for Ubuntu
# This script initializes the ESP-IDF environment on Ubuntu

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== ESP-IDF Ubuntu Setup ===${NC}"

# Install dependencies
echo -e "${BLUE}Step 1: Installing system dependencies...${NC}"
sudo apt-get update
sudo apt-get install -y \
    git wget flex bison gperf python3-setuptools cmake ninja-build ccache \
    libffi-dev libssl-dev dfu-util libusb-1.0-0 python3-pip python3-venv

# Create ESP directory
echo -e "${BLUE}Step 2: Setting up ESP-IDF...${NC}"
mkdir -p ~/esp
cd ~/esp

# Clone ESP-IDF if not already present
if [ ! -d "esp-idf" ]; then
    echo "Cloning ESP-IDF v5.5..."
    git clone -b v5.5 --depth 1 https://github.com/espressif/esp-idf.git
else
    echo "ESP-IDF already cloned"
fi

# Install tools
cd ~/esp/esp-idf
echo "Installing ESP-IDF tools (this may take several minutes)..."
./install.sh esp32s3

# Create alias
echo -e "${BLUE}Step 3: Creating convenient alias...${NC}"
if ! grep -q "alias idf-activate" ~/.bashrc 2>/dev/null; then
    echo "alias idf-activate='source ~/esp/esp-idf/export.sh'" >> ~/.bashrc
    echo -e "${GREEN}✓ Added 'idf-activate' alias to ~/.bashrc${NC}"
fi

echo -e "${GREEN}=== Setup Complete ===${NC}"
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Run: source ~/esp/esp-idf/export.sh"
echo "2. Or use: idf-activate (after restart or 'source ~/.bashrc')"
echo "3. Then: cd /home/titan/aaron/testcv/ESP_PROJECT"
echo "4. Build: idf.py build"
echo "5. Flash: idf.py -p /dev/ttyACM0 flash monitor"
