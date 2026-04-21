# Task: GPIO-46 LOG_SWITCH_PIN verlegen und Profil umbenennen + NTRIP-GNSS-Dependency (Review-F4, F5, F6)

- **Origin:** Kombinierter Review TASK-026..030, Findings F4 (Mittel), F5 (Mittel), F6 (Mittel)
- **Entscheidung Mensch:** LOG_SWITCH_PIN auf anderen GPIO verlegen (F4); Planer-Entscheidung für F5/F6 (klar)

## Kontext / Problem

### F4: GPIO-46 Konflikt — IMU_INT vs LOG_SWITCH_PIN

Auf dem ESP32-S3 wird GPIO-46 von zwei Feature-Modulen beansprucht:
- `IMU_INT = 46` (BNO085 Interrupt-Pin, MOD_IMU)
- `LOG_SWITCH_PIN = 46` (SD-Logging Schalter, MOD_LOGSW)

Per ADR-HAL-001 muss ein Kommentar keine technische Konfliktbehandlung ersetzen. Der Mensch hat entschieden: LOG_SWITCH_PIN auf einen anderen GPIO verlegen.

**Mögliche alternative GPIOs auf ESP32-S3** (bidirektional, nicht von PSRM/ETH/SPI belegt):
- GPIO-3 (aktuell UART1 RX auf S3 = GNSS_UART1_RX=45, aber GPIO-3 ist frei laut Pin-Matrix)
- GPIO-8 (nicht belegt, aber strapping pin — Vorsicht)
- GPIO-48 (GNSS_UART1_TX=48 — belegt!)
- GPIO-2 (GNSS_UART2_TX=2 — belegt!)
- GPIO-1 (GNSS_UART2_RX=1 — belegt!)

**Vorschlag Planer:** GPIO-3 auf ESP32-S3 (freier GPIO, bidirektional, kein Strapping-Konflikt). Der Entwickler muss die ESP32-S3 Pin-Matrix prüfen und einen freien GPIO auswählen, der nicht mit existing Peripherals kollidiert.

**Hinweis ESP32 Classic:** Auf dem ESP32 Classic ist `LOG_SWITCH_PIN = PIN_BT = 0` (Boot-Taste). Kein Konflikt dort — keine Änderung nötig.

### F5: Irreführender Profilname in platformio.ini

Das Profil `[env:profile_full_steer_ntrip_esp32]` trägt `-DFEAT_PROFILE_FULL_STEER` als Flag, enthält aber **kein** Steer-Feature (nur COMM + GNSS + NTRIP auf ESP32 Classic). Dies ist irreführend.

**Umbenennung:** `profile_full_steer_ntrip_esp32` → `profile_ntrip_classic` und `-DFEAT_PROFILE_FULL_STEER` entfernen.

**Achtung:** Es existiert bereits ein Profil `[env:profile_full_steer_ntrip]` (Zeile 137, das den S3 nutzt!). Nach Umbenennung des ESP32-Profils gibt es kein Namenskonflikt mehr.

### F6: NTRIP-GNSS-Dependency fehlt

NTRIP hängt in der Dependency-Chain aktuell nur von ETH ab (FEAT_DEPS_NTRIP = { MOD_ETH, 0xFF }). Logisch sollte NTRIP auch GNSS als Dependency haben — ohne GNSS-Modul macht NTRIP keinen Sinn.

Per ADR-GNSS-001: *"NTRIP-Status muss mit GNSS-/HW-Status konsistent modelliert werden."*

**Fix:** FEAT_DEPS_NTRIP in beiden Board-Profilen um MOD_GNSS ergänzen.

## Akzeptanzkriterien

### F4:
1. LOG_SWITCH_PIN auf ESP32-S3 ist auf einen GPIO verlegt, der nicht mit IMU_INT (46) kollidiert
2. Der neue GPIO ist frei (nicht von ETH, SPI, UART, PSRAM belegt)
3. `moduleActivate(MOD_IMU)` und `moduleActivate(MOD_LOGSW)` können beide成功 (kein Konflikt)
4. ESP32 Classic: Keine Änderung (LOG_SWITCH_PIN bleibt PIN_BT=0)

### F5:
1. Profil `[env:profile_full_steer_ntrip_esp32]` umbenannt zu `[env:profile_ntrip_classic]`
2. `-DFEAT_PROFILE_FULL_STEER` aus dem Profil entfernt
3. Kein anderes Profil referenziert den alten Namen

### F6:
1. FEAT_DEPS_NTRIP in beiden Board-Profilen enthält MOD_GNSS (ID=4) und MOD_ETH (ID=3)
2. `moduleActivate(MOD_NTRIP)` ohne vorheriges `moduleActivate(MOD_GNSS)` schlägt fehl

### Allgemein:
3. Alle Build-Profile kompilieren fehlerfrei
4. `rg "GPIO 46" include/` zeigt IMU_INT=46 aber NICHT LOG_SWITCH_PIN=46

## Scope (in)

- `include/board_profile/LILYGO_T_ETH_LITE_ESP32_S3_board_pins.h`:
  - LOG_SWITCH_PIN auf neuen GPIO ändern
  - FEAT_PINS_LOGSW aktualisieren
  - Kommentar "CONFLICT with IMU_INT!" entfernen
  - FEAT_DEPS_NTRIP um MOD_GNSS ergänzen
- `include/board_profile/LILYGO_T_ETH_LITE_ESP32_board_pins.h`:
  - FEAT_DEPS_NTRIP um MOD_GNSS ergänzen (keine GPIO-Änderung nötig)
- `platformio.ini`:
  - `[env:profile_full_steer_ntrip_esp32]` → `[env:profile_ntrip_classic]`
  - `-DFEAT_PROFILE_FULL_STEER` entfernen
- Ggf. Dokumentation (`README.md` oder Board-Doc) aktualisieren

## Nicht-Scope (out)

- Keine Änderung an ESP32 Classic LOG_SWITCH_PIN (PIN_BT=0 ist korrekt)
- Keine Änderung am Modulsystem-Code (`modules.cpp`)
- Kein Hardware-Redesign (nur Pin-Neuzuweisung im Profil)

## Verifikation / Test

- `pio run -e T-ETH-Lite-ESP32S3` (profile_full_steer) — muss kompilieren
- `pio run -e profile_ntrip_classic` (umbenannt) — muss kompilieren
- `pio run -e profile_full_steer_ntrip` (S3) — muss kompilieren (unverändert)
- `rg "profile_full_steer_ntrip_esp32" platformio.ini` — keine Treffer (alter Name entfernt)

## Relevante ADRs

- **ADR-HAL-001** (Pin-Konflikt-Politik): *"Ein Kommentar im Boardprofil ersetzt keine technische Konfliktbehandlung."* → Wird erfüllt durch GPIO-Verlegung.
- **ADR-003** (Feature-Modulsystem): *"Pin-Konflikte dürfen nicht stillschweigend toleriert werden."* → Erledigt.
- **ADR-GNSS-001** (NTRIP-Policy): *"NTRIP-Status muss mit GNSS-/HW-Status konsistent modelliert werden."* → Wird erfüllt durch GNSS-Dependency.

## Invarianten

- LOG_SWITCH_PIN bleibt "active LOW, internal pull-up" (Konfiguration ändert sich nicht, nur der GPIO)
- FEAT_DEPS_NTRIP Terminator bleibt 0xFF
- MOD_GNSS = 4 (FirmwareFeatureId enum in modules.h)

## Known Traps

1. **ESP32-S3 GPIO-Verfügbarkeit:** GPIO 26-37 sind PSRAM-reserviert, 38-42 output-only, 43-48 bidirektional. Die verfügbaren bidirektionalen GPIOs sind knapp. Der Entwickler muss die aktuelle Pin-Belegung sorgfältig prüfen (Board-Doc: `docs/T-ETH-Lite-S3-pin-en.jpg`).
2. **Profil-Namensänderung:** Wenn der Profilname in der CI/CD-Pipeline, Skripten oder Dokumentation referenziert wird, müssen diese ebenfalls aktualisiert werden. Tool: `rg "profile_full_steer_ntrip_esp32" .` im Projekt durchsuchen.
3. **Strapping-Pins:** GPIO-3, GPIO-45, GPIO-46 sind Strapping-Pins auf ESP32-S3. Strapping-Pins können beim Boot bestimmte Konfigurationen beeinflussen. Der Entwickler muss sicherstellen, dass der gewählte GPIO als Strapping-Pin keine unerwünschten Boot-Effekte hat.
4. **FEAT_DEPS_NTRIP Reihenfolge:** Die Dependency-Reihenfolge im Array spielt keine Rolle (moduleActivate() prüft nur ob alle deps MOD_ON sind). Aber die Reihenfolge der moduleActivate()-Aufrufe in `setup()` muss GNSS vor NTRIP haben — das ist bereits der Fall (Zeile 579 vs. 583 in main.cpp).

## Merge Risk

- **Niedrig bis Mittel:** Pin-Änderung betrifft nur ESP32-S3 Board-Profil. Profil-Umbenennung ist kosmetisch. Dependency-Erweiterung ist ein Array-Eintrag.

## Classification

- **category:** platform_reuse
- **priority:** medium
- **delivery_mode:** firmware_only
- **exclusive_before:** Keine
- **parallelizable_after:** Parallel mit allen anderen TASK-03x

## Owner

- **Assigned:** KI-Entwickler
- **Status:** todo
