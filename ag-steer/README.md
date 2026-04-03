# AgSteer – Autosteer-Steuergerät für AgOpenGPS / AgIO

> Embedded-Firmware für ein landwirtschaftliches Autosteer-Lenksteuergerät auf Basis des **LilyGO T-ETH-Lite-S3** (ESP32-S3-WROOM-1 + W5500 Ethernet).

Das Steuergerät verbindet sich über Ethernet/UDP mit **AgOpenGPS** (bzw. AgIO auf dem Tablet) und steuert einen hydraulischen oder elektrischen Lenkaktuator. Es liest zwei GNSS-Module (Hauptposition + Heading), eine IMU, einen Lenkwinkelsensor und überwacht einen Safety-Eingang.

---

## Quick Start (VS Code + PlatformIO)

```bash
# 1. Projekt klonen
git clone <repo-url> ag-steer
cd ag-steer

# 2. VS Code öffnen (PlatformIO Extension muss installiert sein)
code .

# 3. Firmware kompilieren
#    Menu: PIO → Build

# 4. Firmware flashen (ESP32-S3 per USB angeschlossen)
#    Menu: PIO → Upload

# 5. Seriellen Monitor öffnen
#    Menu: PIO → Serial Monitor  (115200 baud)

# 6. PC-Simulation testen (keine Hardware nötig)
cd pc_sim && make run
```

---

## Projektstruktur (PlatformIO)

```
ag-steer/
├── platformio.ini                  # PlatformIO-Konfiguration (ESP32-S3)
├── README.md                       # Diese Datei
│
├── include/                        # Projektweite Header (globaler Include-Pfad)
│   └── hardware_pins.h             #   GPIO-Pin-Definitionen
│
├── src/                            # ESP32 Firmware Entry Point
│   └── main.cpp                    #   Arduino setup()/loop() + FreeRTOS-Tasks
│
├── lib/                            # Plattformunabhängiger Code (Library Include-Pfad)
│   ├── hal/                        #   Hardware Abstraction Layer (C-API)
│   │   └── hal.h                   #     Alle HW-Funktionen als pure C-Deklarationen
│   ├── hal_esp32/                  #   ESP32-S3 HAL-Implementierung
│   │   ├── hal_impl.h
│   │   └── hal_impl.cpp            #     UART, SPI, W5500, FreeRTOS
│   └── logic/                      #   Reine C++ Logik (keine Arduino-Header)
│       ├── global_state.h/.cpp     #     NavigationState, Mutex, StateLock
│       ├── aog_udp_protocol.h/.cpp #     AOG-Frame-Strukturen, PGNs, Encoder/Decoder
│       ├── gnss.h/.cpp             #     NMEA-Parser (GGA + RMC)
│       ├── imu.h/.cpp              #     BNO085 SPI-Stub (yaw_rate, roll)
│       ├── steer_angle.h/.cpp      #     Lenkwinkelsensor SPI
│       ├── actuator.h/.cpp         #     Aktuator SPI
│       ├── control.h/.cpp          #     PID-Regler + 200Hz-Control-Loop
│       └── net.h/.cpp              #     UDP-Kommunikation mit AgIO
│
└── pc_sim/                         # PC-Simulation (nicht von PlatformIO kompiliert)
    ├── Makefile                    #   g++ Build
    ├── main_pc.cpp                 #   Simulations-Hauptprogramm
    └── hal_pc/                     #   PC-HAL (std::chrono, std::mutex, Dummy-Sensoren)
        ├── hal_impl.h
        └── hal_impl.cpp
```

### Include-Pfad-Auflösung

| Include in Source | Aufgelöst über | Pfad |
|---|---|---|
| `"hal/hal.h"` | `-I lib` | `lib/hal/hal.h` |
| `"hal_esp32/hal_impl.h"` | `-I lib` | `lib/hal_esp32/hal_impl.h` |
| `"logic/global_state.h"` | `-I lib` | `lib/logic/global_state.h` |
| `"global_state.h"` | PlatformIO lib path | `lib/logic/global_state.h` |
| `"hardware_pins.h"` | `include/` (auto) | `include/hardware_pins.h` |

---

## Hardware

### Zielplattform

| Bauteil | Beschreibung |
|---------|-------------|
| **MCU** | ESP32-S3-WROOM-1 (dual-core, 240 MHz, 16 MB Flash, 8 MB OPI PSRAM) |
| **Board** | LilyGO T-ETH-Lite-S3 |
| **Ethernet** | W5500 via SPI (nativ auf dem Board) |
| **GNSS** | 2× UM980 (RTK-Rover, 460800 baud, 8N1) |
| **IMU** | BNO085 (oder kompatibel, SPI) |
| **Lenkwinkelsensor** | SPI-basiert |
| **Aktuator** | SPI-basiert (0–65535 Kommandowert) |
| **Safety** | GPIO5, active LOW (Pull-Up intern) |

> **Hinweis:** Dies ist ausschließlich die S3/W5500-Variante. Kein RMII-Ethernet, keine SC16IS752-UART-Bridge.

### Pin-Belegung

Alle Pins sind zentral in [`include/hardware_pins.h`](include/hardware_pins.h) definiert:

#### SPI Bus 1 – Ethernet (W5500)

| Signal | GPIO | Funktion |
|--------|------|----------|
| ETH_SPI_SCK | 48 | SPI-Takt für W5500 |
| ETH_SPI_MOSI | 21 | Daten MCU → W5500 |
| ETH_SPI_MISO | 47 | Daten W5500 → MCU |
| ETH_CS | 45 | Chip Select W5500 |
| ETH_INT | 14 | Interrupt W5500 |

#### UARTs – GNSS

| Signal | GPIO | Funktion |
|--------|------|----------|
| GNSS_MAIN_TX | 17 | Haupt-GNSS senden |
| GNSS_MAIN_RX | 18 | Haupt-GNSS empfangen |
| GNSS_HEADING_TX | 15 | Heading-GNSS senden |
| GNSS_HEADING_RX | 16 | Heading-GNSS empfangen |

Beide UARTs: **460800 Baud, 8N1**

#### SPI Bus 2 – Sensoren / Aktuator

| Signal | GPIO | Funktion |
|--------|------|----------|
| SENS_SPI_SCK | 12 | SPI-Takt Sensorbus |
| SENS_SPI_MOSI | 11 | Daten MCU → Sensor |
| SENS_SPI_MISO | 13 | Daten Sensor → MCU |
| CS_IMU | 10 | Chip Select IMU (BNO085) |
| CS_STEER_ANG | 9 | Chip Select Lenkwinkelsensor |
| CS_ACT | 8 | Chip Select Aktuator |
| IMU_INT | 6 | Interrupt IMU |
| STEER_ANG_INT | 7 | Interrupt Lenkwinkelsensor |

#### Safety

| Signal | GPIO | Funktion |
|--------|------|----------|
| SAFETY_IN | 5 | Safety-Eingang, **active LOW** |

### Bus-Topologie

```
  ESP32-S3

  ┌─── SPI1 (HSPI) ────┐       ┌─── SPI2 (FSPI) ────────────────────┐
  │  SCK=48  MOSI=21    │       │  SCK=12  MOSI=11   MISO=13        │
  │  MISO=47 CS=45      │       │                                    │
  └──────┬──────────────┘       │  CS=10 → BNO085 (IMU)              │
         │                      │  CS=9  → Lenkwinkelsensor          │
    ┌────┴────┐                 │  CS=8  → Aktuator                   │
    │  W5500  │                 └────────────────────────────────────┘
    │Ethernet │
    └────┬────┘
         │ RJ45
      ───┴─── Netzwerk (AgIO Tablet)

  ┌─── UART1 ──┐   ┌─── UART2 ───┐
  │ RX=18 TX=17│   │ RX=16 TX=15 │
  └─────┬──────┘   └──────┬──────┘
   UM980 #1            UM980 #2
   (Position)          (Heading)
```

---

## Software-Architektur

### Schichtenmodell

```
┌─────────────────────────────────────────────────┐
│                    src/                          │  Entry Point
│             main.cpp (Arduino setup/loop)       │  FreeRTOS-Tasks
├─────────────────────────────────────────────────┤
│                   lib/logic/                     │
│  gnss · imu · steer_angle · actuator · control  │  Reine C++ Logik
│  net · aog_udp_protocol · global_state          │  Keine HW-Abhängigkeit
├─────────────────────────────────────────────────┤
│                lib/hal/ (Schnittstelle)          │  C-API
│  hal_millis · hal_gnss_* · hal_imu_* · hal_net  │
├─────────────────────────────────────────────────┤
│            lib/hal_esp32/  Implementierung       │  Arduino/FreeRTOS
│  HardwareSerial, Ethernet3, SPI, Semaphore      │
├─────────────────────────────────────────────────┤
│            include/hardware_pins.h               │  Pin-Definitionen
└─────────────────────────────────────────────────┘
```

### FreeRTOS-Tasks

Das ESP32-Firmware nutzt zwei FreeRTOS-Tasks, die auf separaten Kernen laufen:

#### controlTask – Core 1 – 200 Hz (5 ms)

```
1. Safety prüfen (GPIO5)  → bei LOW: PID reset, Aktuator=0
2. IMU lesen (SPI2, CS=GPIO10)  → yaw_rate_dps, roll_deg
3. Lenkwinkel lesen (SPI2, CS=GPIO9)  → steer_angle_deg
4. PID berechnen  Fehler = desiredSteerAngleDeg − g_nav.steer_angle_deg
5. Aktuator ansteuern (SPI2, CS=GPIO8)  → uint16_t Kommandowert
```

#### commTask – Core 0 – 100 Hz (10 ms)

```
1. GNSS MAIN pollen (UART1)  → GGA, RMC → lat, lon, alt, sog, cog, fix
2. GNSS HEADING pollen (UART2)  → RMC → heading_deg
3. Netzwerk empfangen (UDP)  → Hello, Scan, SubnetChange, SteerDataIn
4. AOG-Frames senden (@ 10 Hz)  → GPS Main Out + Steer Status Out
```

### Globaler Zustand

Definiert in [`lib/logic/global_state.h`](lib/logic/global_state.h):

```cpp
struct NavigationState {
    double  lat_deg;         float alt_m;       float heading_deg;
    double  lon_deg;         float sog_mps;     float roll_deg;
    float   cog_deg;         float yaw_rate_dps;
    uint8_t fix_quality;     float steer_angle_deg;
    bool    safety_ok;       uint32_t timestamp_ms;
};

extern NavigationState g_nav;
extern volatile float desiredSteerAngleDeg;  // Sollwinkel von AgIO
```

**Thread-Sicherheit:** RAII-Wrapper `StateLock` schützt alle Zugriffe via Mutex.

---

## Komponenten

### GNSS ([`lib/logic/gnss.h`](lib/logic/gnss.h))

| Rolle | UART | Funktion |
|-------|------|----------|
| GNSS_MAIN | UART1 (RX=18, TX=17) | Hauptposition, RTK-Rover |
| GNSS_HEADING | UART2 (RX=16, TX=15) | Heading-Quelle |

NMEA-Parser: **GGA** (lat, lon, alt, fix), **RMC** (sog, cog). 460800 Baud.

### IMU ([`lib/logic/imu.h`](lib/logic/imu.h))

BNO085 über SPI2 (CS=GPIO10). Liest `yaw_rate_dps` und `roll_deg`. SPI-Transaktionsstruktur korrekt, SH-2 Protokoll TODO.

### Lenkwinkelsensor ([`lib/logic/steer_angle.h`](lib/logic/steer_angle.h))

SPI2 (CS=GPIO9). `steerAngleReadDeg()` → Winkel in Grad. Sensorprotokoll TODO.

### Aktuator ([`lib/logic/actuator.h`](lib/logic/actuator.h))

SPI2 (CS=GPIO8). `actuatorWriteCommand(uint16_t cmd)`. Bei `safety_ok == false` → cmd = 0.

### PID-Regler ([`lib/logic/control.h`](lib/logic/control.h))

| Parameter | Standard |
|-----------|----------|
| Kp | 1.0 |
| Ki | 0.0 |
| Kd | 0.01 |
| Output | 0 – 65535 |

Anti-Windup aktiv, Fehler auf [-180°, +180°] gewrappt.

### Netzwerk ([`lib/logic/net.h`](lib/logic/net.h))

- **Sendet @ 10 Hz:** GPS Main Out (Port 5124), Steer Status Out (Port 5126)
- **Empfängt:** Hello (→ Reply), Scan (→ Reply), SubnetChange (→ IP Update), SteerDataIn (→ Sollwinkel)

### HAL ([`lib/hal/hal.h`](lib/hal/hal.h))

Reine C-API mit Implementierungen in `lib/hal_esp32/` und `pc_sim/hal_pc/`. Keine Arduino-Header in der Schnittstelle.

---

## AOG/AgIO UDP-Protokoll

### Frame-Format

```
Offset: 0    1    2    3    4    5...n-2    n-1
       ┌────┬────┬────┬────┬────┬────────┬────┐
       │0x80│0x81│Src │PGN │Len │ Payload│CRC │
       └────┴────┴────┴────┴────┴────────┴────┘

CRC = Low-Byte der Summe von Byte2 bis Byte(n-2)
```

### Unterstützte PGNs

| PGN | Name | Richtung | Payload | Status |
|-----|------|----------|---------|--------|
| 200 | Hello from AgIO | AgIO → Modul | 4 B | Decoder ✓ |
| 201 | Subnet Change | AgIO → Modul | 2 B | Decoder ✓ |
| 202 | Scan Request | AgIO → Modul | 1 B | Decoder ✓ |
| 203 | Subnet Reply | Modul → AgIO | 2 B | Encoder ✓ |
| **254** | **Steer Data In** | **AgIO → Steer** | **8 B** | **Decoder ✓** |
| **253** | **Steer Status Out** | **Steer → AgIO** | **8 B** | **Encoder ✓** |
| **214** | **GPS Main Out** | **GPS → AgIO** | **51 B** | **Encoder ✓** |

GPS Main Out ist mit `static_assert(sizeof(AogGpsMainOut) == 51)` zur Compile-Zeit verifiziert.

---

## Bauen & Testen

### ESP32-Firmware (PlatformIO)

```bash
# Kompilieren
pio run

# Flashen (ESP32-S3 per USB)
pio run --target upload

# Serieller Monitor
pio device monitor -b 115200
```

**Voraussetzungen:**
- VS Code mit PlatformIO Extension
- ESP32-S3 per USB verbunden
- Ethernet3-Bibliothek wird automatisch via `lib_deps` installiert

### PC-Simulation

```bash
cd pc_sim
make          # kompilieren
make run      # ausführen (1s Simulation mit Frame-Verifikation)
make clean    # aufräumen
```

**Erwartete Ergebnisse:**
```
GPS Main Out (PGN 0xD6):  57 bytes  CRC: OK ✓
Steer Status (PGN 0xFD):  14 bytes  CRC: OK ✓
Hello Reply Steer:         8 bytes  CRC: OK ✓

lat:   52.516667°   lon:   13.400000°   fix: 4 (RTK)
SOG:   5.00 m/s     heading: 91.5°
Control loops: 200    Comm loops: 10
```

---

## Konfiguration

### platformio.ini

Wichtige Einstellungen in [`platformio.ini`](platformio.ini):

```ini
board          = esp32-s3-devkitc-1
board_build.flash_mode   = qio
board_build.flash_size   = 16MB
board_build.psram_mode  = opi
lib_deps       = epigrammi/Ethernet3 @ ^1.0.0
upload_speed    = 921600
```

### IP-Konfiguration

Standard-Werte in [`lib/hal_esp32/hal_impl.cpp`](lib/hal_esp32/hal_impl.cpp):

| Parameter | Standard |
|-----------|----------|
| Lokale IP | 192.168.1.70 |
| Subnetz | 255.255.255.0 |
| Gateway | 192.168.1.1 |
| Ziel-IP | 192.168.1.255 (Broadcast) |

Wird automatisch durch **Subnet Change** (PGN 201) von AgIO aktualisiert.

### PID-Parameter

In [`lib/logic/control.cpp`](lib/logic/control.cpp) – `controlInit()`:

```cpp
pidInit(&s_steer_pid, 1.0f, 0.0f, 0.01f, 0.0f, 65535.0f);
```

---

## TODO

| Bereich | Status |
|---------|--------|
| BNO085 SH-2 SPI-Protokoll | 🔲 TODO |
| Echtes Lenkwinkelsensor-Protokoll | 🔲 TODO |
| Echtes Aktuator-Protokoll | 🔲 TODO |
| Dual-Antenna Heading-Fusion | 🔲 TODO |
| SteerSettings In (PGN 252) empfangen | 🔲 TODO |
| SteerConfig In (PGN 251) empfangen | 🔲 TODO |
| HDOP/Satelliten aus GGA | 🔲 TODO |
| FreeRTOS Watchdog | 🔲 TODO |
| OTA-Update | 🔲 TODO |
