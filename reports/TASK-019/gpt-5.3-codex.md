# Entwickler-Report – TASK-019C

**Entwickler:** GPT-5.3-Codex  
**Datum:** 2026-04-17  
**Task-ID:** TASK-019C

## Kurzfassung
Es wurde ein klar aktivierbarer GNSS-Buildup-Modus mit reduziertem Initialisierungspfad umgesetzt. Der Modus initialisiert ausschließlich Kommunikation (ETH/UDP) und GNSS-RTCM-UART, lässt Sensor-/Aktorik-Init aus, liefert diagnostische Start-/Status-Logs und definiert ein reproduzierbares Fallback bei Init-Timeout.

## Gegenüberstellung: Normalmodus vs. GNSS-Buildup-Modus

| Aspekt | Normalmodus | GNSS-Buildup-Modus |
|---|---|---|
| Aktivierung | Standard-Build ohne `FEAT_GNSS_BUILDUP` | Compile-Time-Flag `FEAT_GNSS_BUILDUP` (z. B. via `env:gnss_buildup`) |
| HAL-Init in `setup()` | `hal_esp32_init_all()` | `hal_esp32_init_gnss_buildup()` |
| Subsysteme | Sensor-SPI, IMU, Lenkwinkel, Aktuator, Netzwerk | Nur Netzwerk + GNSS-RTCM-UART |
| Task-Start | `control` + `comm` | Nur `comm` |
| OTA/Kalibrierung/Moduldetektion | Aktiv (normaler Pfad) | Übersprungen (reduzierter Bringup-Pfad) |
| Diagnoselogs | reguläre Laufzeitlogs | explizite GNSS-Buildup-Logs: Init-Status, Port-Status, GNSS-Fix-Status |
| Fallback | normaler Betriebsfluss | Bei Timeout degradierter Diagnosebetrieb ohne Abort/Reboot |

## Technische Details
- Neuer HAL-Entry-Point: `hal_esp32_init_gnss_buildup()` mit ETH-Init und GNSS-RTCM-UART-Init.
- GNSS-Buildup-Defaults (über `-D...` übersteuerbar): UART1, 115200 Baud, RX=45, TX=48.
- `main.cpp` enthält jetzt einen klaren Modusschalter für GNSS-Buildup sowie eine gegenseitige Ausschlussprüfung mit IMU-Bringup.
- Im GNSS-Buildup-Modus werden nur benötigte Pfade ausgeführt; dadurch keine Sensor-/Aktorik-Initialisierung.
- Fallback-Verhalten: nach `MAIN_GNSS_BUILDUP_INIT_TIMEOUT_MS` (15s) ohne `net && rtcm_uart` wird degradierter Bringup-Betrieb geloggt und weitergeführt.

## Verifikation
- Buildaufruf für `gnss_buildup` versucht (`pio run -e gnss_buildup` / `python3 -m platformio run -e gnss_buildup`), jedoch in der Umgebung ohne installiertes PlatformIO-CLI/-Modul nicht ausführbar.
- Regressionstest für Standard-Environment aus gleichem Grund hier nicht lokal ausführbar; Änderungen sind auf klar getrennte Moduspfade begrenzt.
