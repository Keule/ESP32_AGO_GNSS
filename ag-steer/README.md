# AgSteer – Autosteer-Steuergerät für AgOpenGPS / AgIO

> Embedded-Firmware für ein landwirtschaftliches Autosteer-Lenksteuergerät auf Basis des **LilyGO T-ETH-Lite-S3** (ESP32-S3-WROOM-1 + W5500 Ethernet).

Das Steuergerät verbindet sich über Ethernet/UDP mit **AgOpenGPS** (bzw. AgIO auf dem Tablet) und steuert einen hydraulischen oder elektrischen Lenkaktuator. Es liest zwei GNSS-Module (Hauptposition + Heading), eine IMU, einen Lenkwinkelsensor und überwacht einen Safety-Eingang.

---

## Inhaltsverzeichnis

- [Übersicht](#übersicht)
- [Hardware](#hardware)
  - [Zielplattform](#zielplattform)
  - [Pin-Belegung](#pin-belegung)
  - [Bus-Topologie](#bus-topologie)
- [Software-Architektur](#software-architektur)
  - [Verzeichnisstruktur](#verzeichnisstruktur)
  - [Schichtenmodell](#schichtenmodell)
  - [FreeRTOS-Tasks](#freertos-tasks)
  - [Globaler Zustand](#globaler-zustand)
- [Komponenten](#komponenten)
  - [GNSS (gnss)](#gnss-gnss)
  - [IMU (imu)](#imu-imu)
  - [Lenkwinkelsensor (steer_angle)](#lenkwinkelsensor-steer_angle)
  - [Aktuator (actuator)](#aktuator-actuator)
  - [PID-Regler & Control Loop (control)](#pid-regler--control-loop-control)
  - [Netzwerk / AOG-Protokoll (net + aog_udp_protocol)](#netzwerk--aog-protokoll-net--aog_udp_protocol)
  - [HAL – Hardware Abstraction Layer (hal)](#hal--hardware-abstraction-layer-hal)
- [AOG/AgIO UDP-Protokoll](#agagio-udp-protokoll)
  - [Frame-Format](#frame-format)
  - [Unterstützte PGNs](#unterstützte-pgns)
  - [Netzwerk-Discovery](#netzwerk-discovery)
  - [Zyklischer Datenaustausch](#zyklischer-datenaustausch)
- [Bauen & Testen](#bauen--testen)
  - [PC-Simulation](#pc-simulation)
  - [ESP32-Firmware](#esp32-firmware)
- [Konfiguration](#konfiguration)
- [TODO / Weiterentwicklung](#todo--weiterentwicklung)
- [Lizenz](#lizenz)

---

## Übersicht

```
  ┌─────────────┐     Ethernet/UDP      ┌──────────────────┐
  │             │  ◄─── AOG-Protocol ──► │   AgOpenGPS /    │
  │  AgSteer    │                        │   AgIO (Tablet)  │
  │  ┌───────┐  │                        │                  │
  │  │GNSS 1 │──┤── UART1 (460800)      │  NTRIP-Korrektur │
  │  │GNSS 2 │──┤── UART2 (460800)      │  via Internet    │
  │  └───────┘  │                        └──────────────────┘
  │  ┌───────┐  │
  │  │ IMU   │──┤── SPI2 (CS=GPIO10)
  │  │ST.ANG│──┤── SPI2 (CS=GPIO9)       200 Hz     10 Hz
  │  │ACTUAT│──┤── SPI2 (CS=GPIO8)     ┌──────┐  ┌───────┐
  │  └───────┘  │                       │control│  │ comm  │
  │     ◄SAFETY─┤── GPIO5 (active LOW)  │ Task │  │ Task  │
  │             │                       │Core 1 │  │Core 0 │
  └──────┬──────┘                       └──┬───┘  └──┬────┘
         │ W5500 (SPI1)                   │         │
         │ GPIO48/21/47/45                │         │
```

**Aufgaben des Steuergeräts:**

1. Zwei UM980 GNSS-Module auslesen (Position + Heading)
2. IMU-Daten lesen (Gierwinkelgeschwindigkeit, Nickwinkel)
3. Lenkwinkelsensor auslesen
4. PID-Reglung: Sollwinkel von AgIO → Aktuator ansteuern
5. Safety-Eingang überwachen (Not-Aus)
6. GPS- und Lenkstatus an AgIO senden

---

## Hardware

### Zielplattform

| Bauteil | Beschreibung |
|---------|-------------|
| **MCU** | ESP32-S3-WROOM-1 (dual-core, 240 MHz) |
| **Board** | LilyGO T-ETH-Lite-S3 |
| **Ethernet** | W5500 via SPI (nativ auf dem Board) |
| **GNSS** | 2x UM980 (RTK-Rover, 460800 baud, 8N1) |
| **IMU** | BNO085 (oder kompatibel, SPI) |
| **Lenkwinkelsensor** | SPI-basiert |
| **Aktuator** | SPI-basiert (0–65535 Kommandowert) |
| **Safety** | GPIO5, active LOW (Pull-Up intern) |

> **Hinweis:** Dies ist ausschließlich die S3/W5500-Variante. Kein RMII-Ethernet, keine SC16IS752-UART-Bridge.

### Pin-Belegung

Alle Pins sind zentral in [`hardware_pins.h`](hardware_pins.h) definiert:

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

### Verzeichnisstruktur

```
ag-steer/
├── hardware_pins.h                  # GPIO-Pins (zentrale Definition)
│
├── hal/                             # Hardware Abstraction Layer (Schnittstelle)
│   └── hal.h                        #   C-API: alle HW-Funktionen
│
├── hal_esp32/                       # ESP32-S3 Implementierung
│   ├── hal_impl.h
│   └── hal_impl.cpp                 #   Arduino/ESP32: UART, SPI, W5500, FreeRTOS
│
├── hal_pc/                          # PC/Linux Simulation
│   ├── hal_impl.h
│   └── hal_impl.cpp                 #   std::chrono, std::mutex, Dummy-Sensoren
│
├── logic/                           # Reine Logik (keine Arduino-Header!)
│   ├── global_state.h               #   NavigationState, Mutex, StateLock
│   ├── global_state.cpp             #   Globale Instanz g_nav
│   ├── aog_udp_protocol.h           #   AOG-Frame-Strukturen, PGNs, Encoder/Decoder
│   ├── aog_udp_protocol.cpp         #   Checksum, BuildFrame, Encode, Decode
│   ├── gnss.h                       #   GNSS-Modul: NMEA-Parser
│   ├── gnss.cpp                     #   GGA (Position) + RMC (SOG, COG)
│   ├── imu.h                        #   IMU-Treiber (BNO085 SPI)
│   ├── imu.cpp                      #   yaw_rate_dps, roll_deg
│   ├── steer_angle.h                #   Lenkwinkelsensor
│   ├── steer_angle.cpp              #   steeringAngleReadDeg()
│   ├── actuator.h                   #   Aktuator
│   ├── actuator.cpp                 #   actuatorWriteCommand()
│   ├── control.h                    #   PID-Regler + 200Hz-Loop
│   ├── control.cpp                  #   controlStep(), pidCompute()
│   ├── net.h                        #   Netzwerk: UDP-Kommunikation
│   └── net.cpp                      #   netPollReceive(), netSendAogFrames()
│
├── firmware_esp32/                  # ESP32 Firmware
│   └── src/
│       └── main.cpp                 #   Arduino setup()/loop(), FreeRTOS-Tasks
│
├── firmware_pc/                     # PC Simulation
│   └── main_pc.cpp                  #   Simulations-Hauptprogramm mit Frame-Dump
│
└── README.md                        # Diese Datei
```

### Schichtenmodell

```
┌─────────────────────────────────────────────────┐
│              firmware_esp32 / firmware_pc        │  Entry Point
│         (main.cpp / FreeRTOS Tasks / Sim)       │
├─────────────────────────────────────────────────┤
│                    logic/                         │
│  gnss · imu · steer_angle · actuator · control  │  Reine C++ Logik
│  net · aog_udp_protocol · global_state          │  Keine HW-Abhängigkeit
├─────────────────────────────────────────────────┤
│              hal/ (Schnittstelle)                │  C-API
│  hal_millis · hal_gnss_* · hal_imu_* · hal_net  │
├─────────────────────────────────────────────────┤
│           hal_esp32/  oder  hal_pc/              │  Implementierung
│  Arduino/FreeRTOS  oder  std::chrono/std::mutex  │
├─────────────────────────────────────────────────┤
│              hardware_pins.h                     │  Pin-Definitionen
└─────────────────────────────────────────────────┘
```

**Wichtig:** Die `logic/`-Ebene enthält **keine** Arduino- oder ESP32-Header. Sie ist vollständig plattformunabhängig und kann auf PC und ESP32 identisch kompiliert werden.

### FreeRTOS-Tasks

Das ESP32-Firmware nutzt zwei FreeRTOS-Tasks, die auf separaten Kernen laufen:

#### controlTask – Core 1 – 200 Hz (5 ms)

```
┌─────────────────────────────────────────┐
│            controlTask (Core 1)         │
│              alle 5 ms                  │
│                                         │
│  1. Safety prüfen (GPIO5)               │
│     → bei LOW: PID reset, Aktuator=0   │
│                                         │
│  2. IMU lesen (SPI2, CS=GPIO10)         │
│     → yaw_rate_dps, roll_deg           │
│                                         │
│  3. Lenkwinkel lesen (SPI2, CS=GPIO9)  │
│     → steer_angle_deg                  │
│                                         │
│  4. PID berechnen                       │
│     Fehler = desiredSteerAngleDeg       │
│            − g_nav.steer_angle_deg      │
│     Output = Kp·e + Ki·∫e + Kd·ė       │
│                                         │
│  5. Aktuator ansteuern (SPI2, CS=GPIO8) │
│     → uint16_t Kommandowert            │
└─────────────────────────────────────────┘
```

#### commTask – Core 0 – 100 Hz (10 ms)

```
┌─────────────────────────────────────────┐
│            commTask (Core 0)            │
│             alle 10 ms                  │
│                                         │
│  1. GNSS MAIN pollen (UART1)            │
│     → NMEA: GGA, RMC                   │
│     → lat, lon, alt, sog, cog, fix     │
│                                         │
│  2. GNSS HEADING pollen (UART2)         │
│     → NMEA: RMC                        │
│     → heading_deg (COG-Stub)           │
│                                         │
│  3. Netzwerk empfangen (UDP)            │
│     → Hello, Scan, SubnetChange        │
│     → SteerDataIn (→ desiredSteerAngle) │
│                                         │
│  4. AOG-Frames senden (@ 10 Hz)         │
│     → GPS Main Out (PGN 0xD6) Port 5124│
│     → Steer Status Out (PGN 0xFD) 5126 │
└─────────────────────────────────────────┘
```

### Globaler Zustand

Der gesamte Zustand ist in [`logic/global_state.h`](logic/global_state.h) definiert:

```cpp
struct NavigationState {
    // --- GNSS Main ---
    double  lat_deg;        // WGS84 Breitengrad
    double  lon_deg;        // WGS84 Längengrad
    float   alt_m;          // Höhe über Ellipsoid [m]
    float   sog_mps;        // Geschwindigkeit über Grund [m/s]
    float   cog_deg;        // Kurs über Grund [°, 0–360]
    uint8_t fix_quality;    // 0=keine, 1=GPS, 2=DGPS, 4=RTK Fix, 5=RTK Float

    // --- Heading / IMU ---
    float   heading_deg;    // Fusioniert/GNSS Heading [°, 0–360]
    float   roll_deg;       // Nickwinkel [°]
    float   yaw_rate_dps;   // Gierwinkelgeschwindigkeit [°/s]

    // --- Steering ---
    float   steer_angle_deg; // Gemessener Lenkwinkel [°]
    bool    safety_ok;       // true = Safety OK, false = KICK

    // --- Timing ---
    uint32_t timestamp_ms;  // Letzter Update-Zeitstempel [ms]
};

extern NavigationState g_nav;              // Globale Instanz
extern volatile float desiredSteerAngleDeg; // Sollwinkel von AgIO
```

**Thread-Sicherheit:** Alle Zugriffe auf `g_nav` sind mit einem Mutex geschützt. Der RAII-Wrapper `StateLock` stellt sicher, dass der Mutex immer freigegeben wird:

```cpp
{
    StateLock lock;                   // Mutex lock
    float angle = g_nav.steer_angle_deg;
}                                     // automatischer unlock
```

---

## Komponenten

### GNSS (gnss)

**Dateien:** [`logic/gnss.h`](logic/gnss.h), [`logic/gnss.cpp`](logic/gnss.cpp)

Verwaltet zwei UM980 GNSS-Module über Hardware-UARTs:

| Rolle | UART | Funktion |
|-------|------|----------|
| **GNSS_MAIN** | UART1 (RX=18, TX=17) | Hauptposition, RTK-Rover |
| **GNSS_HEADING** | UART2 (RX=16, TX=15) | Heading-Quelle, zweite Antenne |

**NMEA-Parser** unterstützt:

- **GGA** → `lat_deg`, `lon_deg`, `alt_m`, `fix_quality`
- **RMC** → `sog_mps` (Speed over Ground), `cog_deg` (Course over Ground)

Beide UARTs laufen bei 460800 Baud. Die `gnssPollMain()` und `gnssPollHeading()` Funktionen lesen alle verfügbaren NMEA-Zeilen und aktualisieren den globalen Zustand.

> **Status:** Funktionierend. Dual-Antenna-Heading-Fusion ist als Stub angelegt (aktuell wird COG vom Heading-GNSS verwendet).

---

### IMU (imu)

**Dateien:** [`logic/imu.h`](logic/imu.h), [`logic/imu.cpp`](logic/imu.cpp)

BNO085 (oder kompatibel) über SPI Bus 2 (CS=GPIO10, INT=GPIO6).

Liest:
- **`yaw_rate_dps`** – Gierwinkelgeschwindigkeit [°/s]
- **`roll_deg`** – Nickwinkel [°]

Beide Werte werden direkt in `g_nav` geschrieben.

> **Status:** SPI-Transaktionsstruktur steht. BNO085 SH-2 Protokoll ist als Stub/TODO markiert.

---

### Lenkwinkelsensor (steer_angle)

**Dateien:** [`logic/steer_angle.h`](logic/steer_angle.h), [`logic/steer_angle.cpp`](logic/steer_angle.cpp)

SPI Bus 2 (CS=GPIO9, INT=GPIO7).

```cpp
float steerAngleReadDeg(void);   // Gibt aktuellen Lenkwinkel in Grad zurück
```

> **Status:** SPI-CS-Handhabung korrekt. Aktuelles Sensorprotokoll ist als Stub markiert.

---

### Aktuator (actuator)

**Dateien:** [`logic/actuator.h`](logic/actuator.h), [`logic/actuator.cpp`](logic/actuator.cpp)

SPI Bus 2 (CS=GPIO8).

```cpp
void actuatorWriteCommand(uint16_t cmd);  // 0 = Leerlauf, >0 = Lenkbefehl
```

Sicherheit: Wenn `safety_ok == false`, wird automatisch `actuatorWriteCommand(0)` aufgerufen.

> **Status:** SPI-Write mit korrekter CS-Handhabung. Kommandoprotokoll ist Stub.

---

### PID-Regler & Control Loop (control)

**Dateien:** [`logic/control.h`](logic/control.h), [`logic/control.cpp`](logic/control.cpp)

#### PID-Parameter (Standardwerte)

| Parameter | Standard | Beschreibung |
|-----------|----------|-------------|
| Kp | 1.0 | Proportionalverstärkung |
| Ki | 0.0 | Integralverstärkung |
| Kd | 0.01 | Differentialverstärkung |
| Output-Bereich | 0 – 65535 | Aktuator-Kommandobereich |

#### `controlStep()` Ablauf (200 Hz)

```
1. Safety prüfen
   → GPIO5 LOW? → safety_ok = false
   → Aktuator sofort auf 0, PID reset, RETURN

2. IMU lesen
   → g_nav.yaw_rate_dps, g_nav.roll_deg

3. Lenkwinkel lesen
   → g_nav.steer_angle_deg

4. PID berechnen
   Fehler = desiredSteerAngleDeg − g_nav.steer_angle_deg
   Fehler wird auf [-180°, +180°] gewrappt
   Output = PID(Fehler)
   → uint16_t cmd

5. Aktuator ansteuern
   actuatorWriteCommand(cmd)
```

**Anti-Windup:** Der Integralanteil wird begrenzt, um Aufwindeln bei großen Fehlern zu vermeiden.

---

### Netzwerk / AOG-Protokoll (net + aog_udp_protocol)

**Dateien:** [`logic/net.h`](logic/net.h), [`logic/net.cpp`](logic/net.cpp), [`logic/aog_udp_protocol.h`](logic/aog_udp_protocol.h), [`logic/aog_udp_protocol.cpp`](logic/aog_udp_protocol.cpp)

#### `netInit()`
- W5500 Ethernet initialisieren
- UDP-Socket auf Port 9999 (AgIO-Port) öffnen
- Auf Ethernet-Link warten (10s Timeout)

#### `netPollReceive()`
- Alle verfügbaren UDP-Pakete empfangen
- Frame validieren (Preamble, Src, PGN, Len, CRC)
- Decodieren und verarbeiten:
  - **Hello from AgIO** → Antwort mit Steer + GPS Hello Reply
  - **Scan Request** → Antwort mit Subnet Reply
  - **Subnet Change** → Ziel-IP aktualisieren
  - **Steer Data In** → `desiredSteerAngleDeg` aktualisieren
  - Unbekannte PGNs → loggen, aber nicht abstürzen

#### `netSendAogFrames()` (@ 10 Hz)
Zwei Frames werden zyklisch gesendet:

| Frame | Port | Inhalt |
|-------|------|--------|
| **GPS Main Out** (PGN 0xD6) | 5124 | Position, Heading, Speed, Roll, Alt, Fix, IMU-Daten |
| **Steer Status Out** (PGN 0xFD) | 5126 | Ist-Winkel, IMU-Heading, IMU-Roll, Switch, PWM |

---

### HAL – Hardware Abstraction Layer (hal)

**Datei:** [`hal/hal.h`](hal/hal.h)

Die HAL definiert eine reine C-API mit folgenden Kategorien:

| Kategorie | Funktionen |
|-----------|-----------|
| **System** | `hal_millis()`, `hal_micros()`, `hal_delay_ms()` |
| **Logging** | `hal_log(fmt, ...)` – printf-style |
| **Mutex** | `hal_mutex_init/lock/unlock()` – Thread-Safety |
| **Safety** | `hal_safety_ok()` – GPIO5 einlesen |
| **GNSS UART** | `hal_gnss_init()`, `hal_gnss_main_read_line()`, `hal_gnss_heading_read_line()` |
| **SPI Sensoren** | `hal_sensor_spi_init()`, `hal_imu_begin/read()`, `hal_steer_angle_begin/read()`, `hal_actuator_begin/write()` |
| **Netzwerk** | `hal_net_init()`, `hal_net_send()`, `hal_net_receive()`, `hal_net_is_connected()` |

#### Implementierungen

| Backend | Verzeichnis | Platform |
|---------|------------|----------|
| **ESP32-S3** | `hal_esp32/` | Arduino, FreeRTOS, HardwareSerial, Ethernet3, SPI |
| **PC Simulation** | `hal_pc/` | std::chrono, std::mutex, printf, Dummy-Sensoren |

> **Regel:** `hal/hal.h` darf **keine** Arduino- oder ESP32-Header einbinden. Dadurch bleibt die `logic/`-Ebene vollständig plattformunabhängig.

---

## AOG/AgIO UDP-Protokoll

### Frame-Format

Jedes AOG/AgIO-Paket auf dem Ethernet hat dieses Format:

```
Offset  0    1    2    3    4    5 ... n-2    n-1
       ┌────┬────┬────┬────┬────┬────────┬────┐
       │0x80│0x81│Src │PGN │Len │ Payload│CRC │
       └────┴────┴────┴────┴────┴────────┴────┘
        ^Preamble^  ^Header^        ^Data^   ^

Gesamt = 5 (Header) + Len (Payload) + 1 (CRC)
```

- **Preamble:** immer `0x80 0x81`
- **Src:** Quell-Modul (AgIO=0x7F, Steer=0x7E, GPS=0x7C)
- **PGN:** Parameter Group Number (Nachrichtentyp)
- **Len:** Länge der Payload in Bytes
- **CRC:** Low-Byte der Summe aller Bytes von Src bis zum letzten Payload-Byte

### Unterstützte PGNs

| PGN | Hex | Name | Richtung | Payload | Status |
|-----|-----|------|----------|---------|--------|
| 200 | 0xC8 | Hello from AgIO | AgIO → Modul | 4 Bytes | Decoder ✓ |
| 201 | 0xC9 | Subnet Change | AgIO → Modul | 2 Bytes | Decoder ✓ |
| 202 | 0xCA | Scan Request | AgIO → Modul | 1 Byte | Decoder ✓ |
| 203 | 0xCB | Subnet Reply | Modul → AgIO | 2 Bytes | Encoder ✓ |
| 254 | 0xFE | Steer Data In | AgIO → Steer | 8 Bytes | Decoder ✓ |
| 253 | 0xFD | Steer Status Out | Steer → AgIO | 8 Bytes | Encoder ✓ |
| 214 | 0xD6 | GPS Main Out | GPS → AgIO | **51 Bytes** | Encoder ✓ |

#### Hello Replies (PGN = Src)

| Src | PGN | Name | Payload |
|-----|-----|------|---------|
| 0x7E | 0x7E | Hello Reply Steer | 2 Bytes (Adresse) |
| 0x78 | 0x78 | Hello Reply GPS | 2 Bytes (Adresse) |

### Netzwerk-Discovery

Wenn AgIO startet, sendet es **Hello from AgIO** (PGN 200) als Broadcast. Das Steuergerät antwortet:

```
AgIO ──► Hello from AgIO (0xC8, Src=0x7F)
       ◄── Hello Reply Steer (Src=0x7E, PGN=0x7E) auf Port 5126
       ◄── Hello Reply GPS   (Src=0x78, PGN=0x78) auf Port 5124

AgIO ──► Scan Request (0xCA, Src=0x7F)
       ◄── Subnet Reply (0xCB, Src=0x7E) auf Port 5126
       ◄── Subnet Reply (0xCB, Src=0x78) auf Port 5124

AgIO ──► Subnet Change (0xC9) → Aktualisiert Ziel-IP des Moduls
```

### Zyklischer Datenaustausch

```
@ 10 Hz:

  AgIO ──► Steer Data In (PGN 0xFE, 8 Bytes, Port 9999)
           │ speed, status, steerAngle, sectionControl

  ◄────── Steer Status Out (PGN 0xFD, 8 Bytes, Port 5126)
           │ actualSteerAngle, imuHeading, imuRoll, switch, pwm

  ◄────── GPS Main Out (PGN 0xD6, 51 Bytes, Port 5124)
           │ lon, lat, heading, dualHeading, speed, roll, alt,
           │ satCount, fixQuality, hdop, age,
           │ imuHeading, imuRoll, imuPitch, imuYawRate,
           │ relay, sections, time
```

### GPS Main Out Payload (51 Bytes)

| Offset | Größe | Feld | Skalierung |
|--------|-------|------|-----------|
| 0–3 | int32 | Longitude | × 1e7 |
| 4–7 | int32 | Latitude | × 1e7 |
| 8–9 | int16 | Heading | × 16 |
| 10–11 | int16 | Dual Heading | × 16 |
| 12–13 | uint16 | Speed | mm/s |
| 14–15 | int16 | Roll | × 16 |
| 16–19 | int32 | Altitude | mm |
| 20 | uint8 | Satellites | – |
| 21 | uint8 | Fix Quality | – |
| 22–23 | int16 | HDOP | × 100 |
| 24–25 | int16 | Age | × 100 ms |
| 26–27 | int16 | IMU Heading | × 16 |
| 28–29 | int16 | IMU Roll | × 16 |
| 30–31 | int16 | IMU Pitch | × 16 |
| 32–33 | int16 | IMU Yaw Rate | – |
| 34–35 | int16 | Reserved | – |
| 36–39 | uint32 | Relay | – |
| 40–41 | uint8×2 | Sections | – |
| 42–50 | – | Time (Y/M/D/H/M/S/ms) | – |

> **Validierung:** Die Struktur ist mit `static_assert(sizeof(AogGpsMainOut) == 51)` zur Compile-Zeit überprüft.

---

## Bauen & Testen

### PC-Simulation

Die PC-Simulation kompiliert mit einem Standard-g++ (C++17) und benötigt keine externen Bibliotheken:

```bash
cd ag-steer

g++ -std=c++17 -O2 -Wall \
    -I./ -I./logic -I./hal -I./hal_pc \
    firmware_pc/main_pc.cpp \
    hal_pc/hal_impl.cpp \
    logic/global_state.cpp \
    logic/aog_udp_protocol.cpp \
    logic/gnss.cpp \
    logic/imu.cpp \
    logic/steer_angle.cpp \
    logic/actuator.cpp \
    logic/control.cpp \
    logic/net.cpp \
    -o sim -lm -lpthread

./sim
```

**Ausgabe enthält:**
- Initialisierungs-Log aller Subsysteme
- PID-Aktuator-Kommandos (200 Hz)
- Hex-Dumps aller generierten AOG-Frames
- CRC-Verifikation (muss "YES" anzeigen)
- Finaler Zustand (Position, Heading, Lenkwinkel, Fix, Safety)
- Leistungsstatistik (Control-Loops, Comm-Loops)

**Erwartete Ergebnisse:**
```
GPS Main Out (PGN 0xD6):  57 bytes  CRC: OK ✓
Steer Status (PGN 0xFD):  14 bytes  CRC: OK ✓
Hello Reply Steer:         8 bytes  CRC: OK ✓
Hello Reply GPS:           8 bytes  CRC: OK ✓

lat:   52.516667°  (Berlin, simuliertes GGA)
lon:   13.400000°
fix:   4 (RTK Fix)
SOG:   5.00 m/s
heading: 91.5°

Control loops: 200 (target 200)  ← 200 Hz verifiziert
Comm loops:    10 (target 10)   ← 10 Hz verifiziert
```

### ESP32-Firmware

#### Voraussetzungen

- Arduino IDE 2.x oder PlatformIO
- Board: **LilyGO T-ETH-Lite-S3** (ESP32-S3)
- Bibliothek: **Ethernet3** (W5500-Unterstützung für ESP32)

#### Arduino IDE

1. Board-Manager: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Board: "ESP32S3 Dev Module" auswählen
3. Bibliothek "Ethernet3" installieren
4. Dateien aus `firmware_esp32/src/` und alle `logic/`, `hal/`, `hal_esp32/` Dateien zum Projekt hinzufügen
5. Kompilieren und hochladen

#### PlatformIO (empfohlen)

Erstelle eine `platformio.ini`:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    epigrammi/Ethernet3 @ ^1.0.0
monitor_speed = 115200
build_src_filter =
    +<*>
    +<../logic/>
    +<../hal/>
    +<../hal_esp32/>
```

---

## Konfiguration

### IP-Konfiguration

Standard-Werte (in `hal_esp32/hal_impl.cpp`):

| Parameter | Standard |
|-----------|----------|
| Lokale IP | 192.168.1.70 |
| Subnetz | 255.255.255.0 |
| Gateway | 192.168.1.1 |
| Ziel-IP | 192.168.1.255 (Broadcast) |

Die Ziel-IP wird automatisch durch **Subnet Change** (PGN 201) von AgIO aktualisiert.

### PID-Parameter

Standard-PID in `logic/control.cpp` – `controlInit()`:

```cpp
pidInit(&s_steer_pid,
    1.0f,    // Kp – Proportional
    0.0f,    // Ki – Integral
    0.01f,   // Kd – Differential
    0.0f,    // Min-Ausgang
    65535.0f // Max-Ausgang
);
```

Diese Werte müssen für den jeweiligen Aktuator/Sensor angepasst werden.

### UART-Baudrate

In [`hardware_pins.h`](hardware_pins.h):

```c
#define GNSS_BAUD_RATE  460800
```

---

## TODO / Weiterentwicklung

| Bereich | Status | Beschreibung |
|---------|--------|-------------|
| **BNO085 SPI-Protokoll** | 🔲 TODO | Vollständiger SH-2 Sensor-Hub-Stack implementieren |
| **Lenkwinkelsensor** | 🔲 TODO | Echtes Sensorprotokoll über SPI (aktuell Stub) |
| **Aktuator-Protokoll** | 🔲 TODO | Spezifisches Kommandoprotokoll implementieren |
| **Dual-Antenna Heading** | 🔲 TODO | Heading-Fusion aus zwei GNSS-Antennen (aktuell COG-Stub) |
| **SteerSettings In** (PGN 252) | 🔲 TODO | PID-Parameter per AgIO konfigurierbar machen |
| **SteerConfig In** (PGN 251) | 🔲 TODO | Fahrzeug-/Sensor-Konfiguration empfangen |
| **UTM-Zeitraum** | 🔲 TODO | In AogGpsMainOut einbauen |
| **HDOP aus GGA** | 🔲 TODO | Aktuell Hardcoded (100) |
| **Satellitenanzahl** | 🔲 TODO | Aus GGA extrahieren |
| **Watchdog** | 🔲 TODO | FreeRTOS Watchdog Timer für beide Tasks |
| **OTA-Update** | 🔲 TODO | Over-the-Air Firmware-Update über Ethernet |

---

## Lizenz

Dieses Projekt wurde für den Einsatz in der landwirtschaftlichen Automatisierung entwickelt.
