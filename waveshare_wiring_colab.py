# ═══════════════════════════════════════════════════════════════════════════
#  IdeaTrac — WaveShare ESP32-S3-A / SIM7670X-4G  Wiring Visualiser
# ═══════════════════════════════════════════════════════════════════════════
#  Upload to Google Colab →  Runtime → Run all
#
#  CONTEXT:
#    The original code ran on an ESP32 NodeMCU (Arduino/PlatformIO) with:
#      - EmonLib           → CT clamp RMS current
#      - MPU6050 (ElectronicCats) → Accel/Gyro via I2C
#      - OneWire + DallasTemperature → DS18B20 1-Wire temp
#      (also: HFT thermistor, BPW85C phototransistor, Reed switch, RS485)
#
#    The WaveShare ESP32-S3-A port uses ESP-IDF v5.5 (not Arduino).
#    All Arduino libraries are replaced with native ESP-IDF drivers:
#      - EmonLib        → custom ADC RMS algorithm in current.c
#      - MPU6050 lib    → raw I2C register read in mpu6050.c
#      - OneWire+Dallas → GPIO bit-bang 1-Wire in ds18b20.c
#      - analogRead()   → adc_oneshot API for thermistor + phototransistor
#
#    The WaveShare board has an ON-BOARD SIM7670X 4G modem on GPIO43/44.
#    Pin numbers are COMPLETELY DIFFERENT from the NodeMCU.
# ═══════════════════════════════════════════════════════════════════════════

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch

# ─────────────────────────────────────────────────────────────────────────
#  SECTION 1 :  NodeMCU → WaveShare Pin Migration Table
# ─────────────────────────────────────────────────────────────────────────

MIGRATION = [
    # (Sensor,            NodeMCU GPIO, WaveShare GPIO, Protocol, Arduino Lib → ESP-IDF)
    ("I2C SDA (MPU6050)", "GPIO 21", "GPIO 9", "I2C", "Wire.h → i2c_master.h"),
    ("I2C SCL (MPU6050)", "GPIO 22", "GPIO 6", "I2C", "Wire.h → i2c_master.h"),
    ("MPU6050 INT", "GPIO 23", "— (unused)", "IRQ", "digitalRead → —"),
    ("CT Clamp (Current)", "GPIO 36 (VP)", "GPIO 2", "ADC", "EmonLib → current.c"),
    ("Phototransistor", "GPIO 39 (VN)", "GPIO 3", "ADC", "analogRead → adc_oneshot"),
    ("Thermistor (HFT)", "— (N/A)", "GPIO 1", "ADC", "— → temperature.c"),
    ("DS18B20 Data", "GPIO 25", "GPIO 8", "1-Wire", "OneWire+Dallas → ds18b20.c"),
    ("Reed Switch DO", "GPIO 13", "GPIO 10", "Digital", "digitalRead → gpio ISR"),
    ("RS485 TX (DI)", "GPIO 17", "GPIO 17", "UART", "Serial2 → uart driver"),
    ("RS485 RX (RO)", "GPIO 16", "GPIO 18", "UART", "Serial2 → uart driver"),
    ("RS485 DE/RE", "GPIO 4", "GPIO 15", "Digital", "digitalWrite → gpio"),
    ("SIM7670X TX", "— (N/A)", "GPIO 43", "UART", "— → sim7670x.c (on-board)"),
    ("SIM7670X RX", "— (N/A)", "GPIO 44", "UART", "— → sim7670x.c (on-board)"),
]


def print_migration_table():
    print("=" * 90)
    print("  NodeMCU ESP-32S  →  WaveShare ESP32-S3-A  Pin Migration")
    print("=" * 90)
    print(
        f"  {'Sensor':<22} {'NodeMCU':<14} {'WaveShare':<14} {'Proto':<8} {'Lib Migration'}"
    )
    print("-" * 90)
    for name, old, new, proto, lib in MIGRATION:
        marker = (
            " ✦"
            if old != new and old != "— (N/A)"
            else "  " if old == "— (N/A)" else " ="
        )
        print(f" {marker} {name:<22} {old:<14} {new:<14} {proto:<8} {lib}")
    print("-" * 90)
    print("  ✦ = pin changed    = = same    (blank) = new on WaveShare")
    print("=" * 90)


# ─────────────────────────────────────────────────────────────────────────
#  SECTION 2 :  WaveShare ESP32-S3-A Physical Pinout
# ─────────────────────────────────────────────────────────────────────────
#  Based on the Waveshare ESP32-S3-A wiki pinout diagram.
#  Left header (top to bottom) and right header (top to bottom).

LEFT_HEADER = [
    ("3V3", "pwr"),
    ("3V3", "pwr"),
    ("RST", "sys"),
    ("GPIO 4", "free"),
    ("GPIO 5", "free"),
    ("GPIO 6", "used"),  # I2C SCL
    ("GPIO 7", "free"),
    ("GPIO 15", "used"),  # RS485 DE/RE
    ("GPIO 16", "free"),
    ("GPIO 17", "used"),  # RS485 TX
    ("GPIO 18", "used"),  # RS485 RX
    ("GPIO 8", "used"),  # DS18B20
    ("GPIO 3", "used"),  # Phototransistor ADC
    ("GPIO 46", "free"),
    ("GPIO 9", "used"),  # I2C SDA
    ("GPIO 10", "used"),  # Reed switch
    ("GPIO 11", "free"),
    ("GPIO 12", "free"),
    ("GPIO 13", "free"),
    ("GPIO 14", "free"),
    ("GND", "gnd"),
]

RIGHT_HEADER = [
    ("5V", "pwr"),
    ("GND", "gnd"),
    ("GPIO 43", "used"),  # SIM7670X TX (on-board)
    ("GPIO 44", "used"),  # SIM7670X RX (on-board)
    ("GPIO 1", "used"),  # Thermistor ADC
    ("GPIO 2", "used"),  # CT clamp ADC
    ("GPIO 42", "free"),
    ("GPIO 41", "free"),
    ("GPIO 40", "free"),
    ("GPIO 39", "free"),
    ("GPIO 38", "free"),
    ("GPIO 37", "free"),
    ("GPIO 36", "free"),
    ("GPIO 35", "free"),
    ("GPIO 0", "sys"),
    ("GPIO 45", "free"),
    ("GPIO 48", "free"),
    ("GPIO 47", "free"),
    ("GPIO 21", "free"),
    ("GPIO 20", "free"),
    ("GPIO 19", "free"),
]

# What each used pin does
PIN_ROLES = {
    "GPIO 1": "Thermistor\n(ADC1_CH0)",
    "GPIO 2": "CT Clamp\n(ADC1_CH1)",
    "GPIO 3": "Photo BPW85C\n(ADC1_CH2)",
    "GPIO 6": "I2C SCL\n(MPU6050)",
    "GPIO 8": "DS18B20\n(1-Wire)",
    "GPIO 9": "I2C SDA\n(MPU6050)",
    "GPIO 10": "Reed Switch\n(ext1 wake)",
    "GPIO 15": "RS485 DE/RE\n(direction)",
    "GPIO 17": "RS485 TX\n(MAX485 DI)",
    "GPIO 18": "RS485 RX\n(MAX485 RO)",
    "GPIO 43": "SIM7670X TX\n(on-board)",
    "GPIO 44": "SIM7670X RX\n(on-board)",
}

# Wire colours per role
WIRE_COLOURS = {
    "GPIO 1": "#FF6D00",  # orange - analog
    "GPIO 2": "#E65100",  # dark orange - analog
    "GPIO 3": "#FFB300",  # amber - analog
    "GPIO 6": "#1565C0",  # blue - I2C
    "GPIO 8": "#2E7D32",  # green - 1-Wire
    "GPIO 9": "#42A5F5",  # light blue - I2C
    "GPIO 10": "#00897B",  # teal - digital
    "GPIO 15": "#7B1FA2",  # purple - RS485
    "GPIO 17": "#9C27B0",  # purple - RS485
    "GPIO 18": "#AB47BC",  # light purple - RS485
    "GPIO 43": "#6A1B9A",  # deep purple - modem
    "GPIO 44": "#8E24AA",  # purple - modem
}


# ─────────────────────────────────────────────────────────────────────────
#  SECTION 3 :  Sensor Wiring Cards
# ─────────────────────────────────────────────────────────────────────────

SENSORS_LEFT = [
    {
        "name": "MPU-6050",
        "subtitle": "Accelerometer / Gyroscope",
        "arduino_lib": "ElectronicCats/MPU6050",
        "espidf_driver": "mpu6050.c (raw I2C regs)",
        "wires": [
            ("VCC", "3V3", "#F44336"),
            ("GND", "GND", "#212121"),
            ("SDA", "GPIO 9", "#42A5F5"),
            ("SCL", "GPIO 6", "#1565C0"),
            ("AD0", "GND", "#212121"),
        ],
        "notes": "I2C 0x68 · 100 kHz · ±2g / ±250°/s",
        "color": "#1976D2",
    },
    {
        "name": "SCT013-030",
        "subtitle": "CT Clamp (via Shield X1)",
        "arduino_lib": "EmonLib (calcIrms)",
        "espidf_driver": "current.c (ADC RMS loop)",
        "wires": [
            ("CT +", "Shield X1 bias", "#E65100"),
            ("CT −", "Shield X1 GND", "#212121"),
            ("Signal", "GPIO 2 (CH1)", "#E65100"),
        ],
        "notes": "30A/1V · bias@VCC/2 · cal 30.0 A/V",
        "color": "#E65100",
    },
    {
        "name": "HFT 105.5/2.7",
        "subtitle": "Thermistor (via Shield A1)",
        "arduino_lib": "analogRead() only",
        "espidf_driver": "temperature.c (β equation)",
        "wires": [
            ("3V3 side", "3V3 → 10kΩ", "#F44336"),
            ("GND side", "GND", "#212121"),
            ("Signal", "GPIO 1 (CH0)", "#FF6D00"),
        ],
        "notes": "β=3950 · R₀=10kΩ@25°C · 12-bit ADC",
        "color": "#FF6D00",
    },
    {
        "name": "BPW85C",
        "subtitle": "Phototransistor (via Shield X2)",
        "arduino_lib": "analogRead() only",
        "espidf_driver": "phototransistor.c (ADC+cal)",
        "wires": [
            ("Collector", "3V3 → load R", "#F44336"),
            ("Emitter", "GND", "#212121"),
            ("Signal", "GPIO 3 (CH2)", "#FFB300"),
        ],
        "notes": "Higher V = more light · raw + mV",
        "color": "#FFB300",
    },
]

SENSORS_RIGHT = [
    {
        "name": "DS18B20",
        "subtitle": "1-Wire Digital Temp",
        "arduino_lib": "OneWire + DallasTemperature",
        "espidf_driver": "ds18b20.c (GPIO bit-bang)",
        "wires": [
            ("Red (VCC)", "3V3", "#F44336"),
            ("Black (GND)", "GND", "#212121"),
            ("Yellow (Data)", "GPIO 8", "#2E7D32"),
        ],
        "notes": "OD+pull-up · 4.7kΩ ext for >1m\n12-bit = 0.0625°C · CRC-8 checked",
        "color": "#2E7D32",
    },
    {
        "name": "Reed Switch",
        "subtitle": "KY-025 Module",
        "arduino_lib": "digitalRead()",
        "espidf_driver": "reed_switch.c (GPIO ISR)",
        "wires": [
            ("VCC", "3V3", "#F44336"),
            ("GND", "GND", "#212121"),
            ("DO", "GPIO 10", "#00897B"),
        ],
        "notes": "Active-LOW · 20ms debounce\next1 deep-sleep wake source",
        "color": "#00897B",
    },
    {
        "name": "MAX485 / RS485",
        "subtitle": "Energy Monitor Shield",
        "arduino_lib": "Serial2 (HW UART)",
        "espidf_driver": "UART1 driver (9600 baud)",
        "wires": [
            ("DI (TX)", "GPIO 17", "#9C27B0"),
            ("RO (RX)", "GPIO 18", "#AB47BC"),
            ("DE/RE", "GPIO 15", "#7B1FA2"),
            ("VCC", "3V3/5V", "#F44336"),
            ("GND", "GND", "#212121"),
        ],
        "notes": "DE/RE: HIGH=TX, LOW=RX\nA/B → external meter (optional)",
        "color": "#7B1FA2",
    },
    {
        "name": "SIM7670X 4G",
        "subtitle": "On-board WaveShare modem",
        "arduino_lib": "— (not on NodeMCU)",
        "espidf_driver": "sim7670x.c (AT commands)",
        "wires": [
            ("ESP TX→Modem RX", "GPIO 43", "#6A1B9A"),
            ("ESP RX←Modem TX", "GPIO 44", "#8E24AA"),
        ],
        "notes": "UART2 @ 115200 · FIXED on board\nUse USB-JTAG console (not UART0)\nSIM required for 4G uplink",
        "color": "#6A1B9A",
    },
]


# ─────────────────────────────────────────────────────────────────────────
#  DRAWING FUNCTIONS
# ─────────────────────────────────────────────────────────────────────────


def _pin_color(label, status):
    if status == "pwr":
        return "#F44336"
    if status == "gnd":
        return "#424242"
    if status == "used":
        return "#FFD600"
    if status == "sys":
        return "#78909C"
    return "#607D8B"


def draw_board(ax):
    """Draw the ESP32-S3 WaveShare board body + pin headers."""
    n_pins = len(LEFT_HEADER)
    board_h = n_pins * 0.42 + 0.8
    bx, by = 4.0, 0.3

    # PCB
    pcb = FancyBboxPatch(
        (bx, by),
        3.0,
        board_h,
        boxstyle="round,pad=0.12",
        facecolor="#1B2631",
        edgecolor="#34495E",
        linewidth=2.5,
    )
    ax.add_patch(pcb)

    # Antenna stub at top
    ax.add_patch(
        FancyBboxPatch(
            (bx + 0.8, by + board_h - 0.05),
            1.4,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor="#2C3E50",
            edgecolor="#455A64",
            lw=1.2,
        )
    )
    ax.text(
        bx + 1.5,
        by + board_h + 0.25,
        "4G ANT",
        ha="center",
        fontsize=5,
        color="#95A5A6",
        family="monospace",
    )

    # USB-C at bottom
    ax.add_patch(
        FancyBboxPatch(
            (bx + 0.9, by - 0.35),
            1.2,
            0.45,
            boxstyle="round,pad=0.05",
            facecolor="#607D8B",
            edgecolor="#455A64",
            lw=1.5,
        )
    )
    ax.text(
        bx + 1.5,
        by - 0.13,
        "USB-C",
        ha="center",
        fontsize=5,
        color="white",
        family="monospace",
    )

    # Board labels
    mid_y = by + board_h / 2
    ax.text(
        bx + 1.5,
        mid_y + 1.5,
        "ESP32-S3",
        ha="center",
        fontsize=11,
        fontweight="bold",
        color="white",
        family="monospace",
    )
    ax.text(
        bx + 1.5,
        mid_y + 1.0,
        "WaveShare",
        ha="center",
        fontsize=9,
        color="#B0BEC5",
        family="monospace",
    )
    ax.text(
        bx + 1.5,
        mid_y + 0.5,
        "ESP32-S3-A",
        ha="center",
        fontsize=7,
        color="#78909C",
        family="monospace",
    )
    ax.text(
        bx + 1.5,
        mid_y + 0.05,
        "SIM7670X-4G",
        ha="center",
        fontsize=7,
        color="#78909C",
        family="monospace",
    )

    # SIM slot indicator
    ax.add_patch(
        FancyBboxPatch(
            (bx + 2.1, mid_y - 1.5),
            0.7,
            0.4,
            boxstyle="round,pad=0.03",
            facecolor="#37474F",
            edgecolor="#546E7A",
            lw=0.8,
        )
    )
    ax.text(
        bx + 2.45,
        mid_y - 1.3,
        "SIM",
        ha="center",
        fontsize=4,
        color="#90A4AE",
        family="monospace",
    )

    # Pin headers
    for i, ((lbl_l, stat_l), (lbl_r, stat_r)) in enumerate(
        zip(LEFT_HEADER, RIGHT_HEADER)
    ):
        y = by + board_h - 0.6 - i * 0.42

        # Left pin dot + label
        lc = _pin_color(lbl_l, stat_l)
        ax.plot(
            bx,
            y,
            "s",
            color=lc,
            markersize=6,
            markeredgecolor="white",
            markeredgewidth=0.4,
            zorder=5,
        )
        weight = "bold" if stat_l == "used" else "normal"
        ax.text(
            bx - 0.15,
            y,
            lbl_l,
            ha="right",
            va="center",
            fontsize=5.5,
            fontweight=weight,
            color=lc,
            family="monospace",
        )
        # Role annotation for used pins
        if lbl_l in PIN_ROLES:
            ax.text(
                bx - 2.4,
                y,
                PIN_ROLES[lbl_l],
                ha="right",
                va="center",
                fontsize=4.5,
                color=WIRE_COLOURS.get(lbl_l, "#999"),
                family="monospace",
                fontweight="bold",
                bbox=dict(
                    boxstyle="round,pad=0.15",
                    facecolor="white",
                    edgecolor=WIRE_COLOURS.get(lbl_l, "#CCC"),
                    alpha=0.9,
                    lw=0.8,
                ),
            )
            ax.annotate(
                "",
                xy=(bx - 0.2, y),
                xytext=(bx - 1.5, y),
                arrowprops=dict(
                    arrowstyle="-",
                    color=WIRE_COLOURS.get(lbl_l, "#999"),
                    lw=1.2,
                    ls="--",
                ),
            )

        # Right pin dot + label
        rc = _pin_color(lbl_r, stat_r)
        ax.plot(
            bx + 3.0,
            y,
            "s",
            color=rc,
            markersize=6,
            markeredgecolor="white",
            markeredgewidth=0.4,
            zorder=5,
        )
        weight = "bold" if stat_r == "used" else "normal"
        ax.text(
            bx + 3.15,
            y,
            lbl_r,
            ha="left",
            va="center",
            fontsize=5.5,
            fontweight=weight,
            color=rc,
            family="monospace",
        )
        if lbl_r in PIN_ROLES:
            ax.text(
                bx + 5.4,
                y,
                PIN_ROLES[lbl_r],
                ha="left",
                va="center",
                fontsize=4.5,
                color=WIRE_COLOURS.get(lbl_r, "#999"),
                family="monospace",
                fontweight="bold",
                bbox=dict(
                    boxstyle="round,pad=0.15",
                    facecolor="white",
                    edgecolor=WIRE_COLOURS.get(lbl_r, "#CCC"),
                    alpha=0.9,
                    lw=0.8,
                ),
            )
            ax.annotate(
                "",
                xy=(bx + 3.2, y),
                xytext=(bx + 4.5, y),
                arrowprops=dict(
                    arrowstyle="-",
                    color=WIRE_COLOURS.get(lbl_r, "#999"),
                    lw=1.2,
                    ls="--",
                ),
            )

    return by, board_h


def _draw_card(ax, x, y, w, h, sensor):
    """Render one sensor wiring card."""
    card = FancyBboxPatch(
        (x, y - h),
        w,
        h,
        boxstyle="round,pad=0.08",
        facecolor="#FAFAFA",
        edgecolor=sensor["color"],
        linewidth=2,
        alpha=0.97,
    )
    ax.add_patch(card)

    # Header bar
    ax.add_patch(
        FancyBboxPatch(
            (x, y - 0.50),
            w,
            0.50,
            boxstyle="round,pad=0.06",
            facecolor=sensor["color"],
            edgecolor="none",
        )
    )
    ax.text(
        x + w / 2,
        y - 0.15,
        sensor["name"],
        ha="center",
        va="center",
        fontsize=7.5,
        fontweight="bold",
        color="white",
        family="monospace",
    )
    ax.text(
        x + w / 2,
        y - 0.38,
        sensor["subtitle"],
        ha="center",
        va="center",
        fontsize=5.5,
        color="#FFFFFFCC",
        family="monospace",
    )

    # Library migration line
    py = y - 0.62
    ax.text(
        x + 0.1,
        py,
        f'Arduino: {sensor["arduino_lib"]}',
        fontsize=4.3,
        color="#B0BEC5",
        family="monospace",
    )
    py -= 0.15
    ax.text(
        x + 0.1,
        py,
        f'ESP-IDF: {sensor["espidf_driver"]}',
        fontsize=4.3,
        color=sensor["color"],
        family="monospace",
        fontweight="bold",
    )
    py -= 0.22

    # Wire table
    for pin_lbl, target, wcolor in sensor["wires"]:
        ax.plot(x + 0.12, py, "o", color=wcolor, markersize=4.5, zorder=5)
        ax.text(
            x + 0.28,
            py,
            pin_lbl,
            ha="left",
            va="center",
            fontsize=5,
            family="monospace",
            color="#37474F",
        )
        ax.text(
            x + w - 0.08,
            py,
            f"→ {target}",
            ha="right",
            va="center",
            fontsize=5,
            family="monospace",
            color=wcolor,
            fontweight="bold",
        )
        py -= 0.2

    # Notes
    if sensor.get("notes"):
        py -= 0.06
        for line in sensor["notes"].split("\n"):
            ax.text(
                x + 0.1,
                py,
                line,
                fontsize=4.2,
                color="#90A4AE",
                family="monospace",
                style="italic",
            )
            py -= 0.14


def draw_sensor_cards(ax, board_y, board_h):
    """Place sensor cards on left and right sides of the board."""
    cw, ch = 2.8, 2.5
    left_x = -3.2
    right_x = 10.2

    y_top = board_y + board_h - 0.2
    spacing = ch + 0.25

    for i, s in enumerate(SENSORS_LEFT):
        _draw_card(ax, left_x, y_top - i * spacing, cw, ch, s)

    for i, s in enumerate(SENSORS_RIGHT):
        _draw_card(ax, right_x, y_top - i * spacing, cw, ch, s)


def draw_legend(ax):
    """Wire colour legend and power rail note."""
    items = [
        ("3V3 Power", "#F44336"),
        ("GND", "#424242"),
        ("I2C (SDA/SCL)", "#1976D2"),
        ("Analog (ADC)", "#FF6D00"),
        ("Digital/1-Wire", "#2E7D32"),
        ("UART / RS485", "#7B1FA2"),
        ("On-board modem", "#6A1B9A"),
    ]
    y = -0.8
    for i, (label, color) in enumerate(items):
        x = -2.8 + i * 2.1
        ax.plot(x, y, "s", color=color, markersize=7)
        ax.text(
            x + 0.18,
            y,
            label,
            va="center",
            fontsize=5.3,
            color="#37474F",
            family="monospace",
        )

    # Power note
    ax.text(
        5.5,
        -1.4,
        "All sensors share 3V3 + GND rails  │  "
        "Deep sleep: timer wake @ 06:00 SAST + ext1 reed GPIO 10",
        ha="center",
        fontsize=5.5,
        color="#B71C1C",
        family="monospace",
        fontweight="bold",
        bbox=dict(
            boxstyle="round,pad=0.25", facecolor="#FFEBEE", edgecolor="#E57373", lw=1
        ),
    )


# ─────────────────────────────────────────────────────────────────────────
#  SECTION 4 :  Sleep / Wake-up Info
# ─────────────────────────────────────────────────────────────────────────


def print_sleep_info():
    print("\n" + "=" * 72)
    print("  DEEP SLEEP / WAKE-UP  (WaveShare ESP32-S3-A)")
    print("=" * 72)
    print(
        """
  Schedule (SAST = UTC+2):
    ACTIVE : 06:00 – 18:00  (12 h)
    SLEEP  : 18:00 – 06:00  (12 h deep sleep, RTC timer)

  Wake-up sources:
    1. TIMER  — RTC alarm fires at 06:00 SAST
    2. EXT1   — Reed switch GPIO 10 pulled LOW
               (magnetic event during sleep)

  Kconfig toggles:
    CONFIG_SLEEP_ENABLED      = y
    CONFIG_SLEEP_WAKE_ON_REED = y

  Power (approximate):
    Active + modem : ~150–300 mA
    Deep sleep     : ~10 µA  (RTC + ext1)

  NOTE: SIM7670X modem is NOT powered in deep sleep.
        On wake it re-initialises (~10–15 s).
    """
    )
    print("=" * 72)


# ─────────────────────────────────────────────────────────────────────────
#  SECTION 5 :  PlatformIO → ESP-IDF Library Mapping
# ─────────────────────────────────────────────────────────────────────────


def print_library_mapping():
    print("\n" + "=" * 80)
    print("  PlatformIO lib_deps (NodeMCU)  →  ESP-IDF native (WaveShare)")
    print("=" * 80)
    libs = [
        (
            "openenergymonitor/EmonLib@^1.1.0",
            "current.c",
            "Custom ADC RMS loop (1480 samples), adc_oneshot + curve-fit cal",
        ),
        (
            "electroniccats/MPU6050@^1.4.4",
            "mpu6050.c",
            "Raw I2C register R/W via i2c_master.h, WHO_AM_I + 14-byte burst",
        ),
        (
            "paulstoffregen/OneWire@^2.3.8",
            "ds18b20.c",
            "GPIO bit-bang (open-drain), µs timing in IRAM critical sections",
        ),
        (
            "milesburton/DallasTemperature@^3.11.0",
            "ds18b20.c",
            "Skip ROM + Convert T + Read Scratchpad, CRC-8 validation",
        ),
    ]
    for pio, idf, detail in libs:
        print(f"\n  Arduino/PIO : {pio}")
        print(f"  ESP-IDF     : {idf}")
        print(f"  How         : {detail}")
    print("\n" + "=" * 80)


# ─────────────────────────────────────────────────────────────────────────
#  MAIN
# ─────────────────────────────────────────────────────────────────────────


def generate_diagram():
    fig, ax = plt.subplots(figsize=(16, 13))
    ax.set_xlim(-4, 15)
    ax.set_ylim(-2, 11)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.patch.set_facecolor("white")

    # Title
    ax.text(
        5.5,
        10.6,
        "IdeaTrac — WaveShare ESP32-S3-A / SIM7670X-4G",
        ha="center",
        fontsize=15,
        fontweight="bold",
        color="#0D47A1",
        family="monospace",
    )
    ax.text(
        5.5,
        10.25,
        "Sensor Wiring Diagram   (ported from NodeMCU Arduino → ESP-IDF v5.5)",
        ha="center",
        fontsize=8,
        color="#5C6BC0",
        family="monospace",
    )

    by, bh = draw_board(ax)
    draw_sensor_cards(ax, by, bh)
    draw_legend(ax)

    plt.tight_layout()
    plt.savefig(
        "waveshare_wiring_diagram.png", dpi=200, bbox_inches="tight", facecolor="white"
    )
    plt.show()
    print("\n✅ Saved: waveshare_wiring_diagram.png")


if __name__ == "__main__":
    print_migration_table()
    print_library_mapping()
    print_sleep_info()
    generate_diagram()
