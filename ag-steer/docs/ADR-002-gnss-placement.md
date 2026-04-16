# ADR-002: GNSS im steer-controller integrieren (statt separates gps-bridge Ziel)

- Status: Accepted
- Datum: 2026-04-16
- Entscheider: Firmware-Team AgSteer

## Kontext

Es gab zwei mögliche Architekturpfade:

1. GNSS als separates Firmware-Ziel `gps-bridge` betreiben und die Daten extern an AgIO weiterreichen.
2. GNSS direkt im `steer-controller` integrieren, sodass Position/Motion/Fix intern verarbeitet werden.

Bestehende Referenzlogik liegt unter `ag-steer/gps-bridge/reference/gnss.*`, während der produktive Laufzeitpfad
im `steer-controller` bereits über Feature-Flags, Task-Scheduling und Freshness-Policies strukturiert ist.

## Entscheidung

Wir **integrieren GNSS in den steer-controller** und behandeln GNSS als optionale Capability `FEAT_GNSS`.

## Gründe

- Ein gemeinsamer `NavigationState` reduziert doppelte Zustandsführung.
- Freshness/Quality kann zentral über `dependency_policy.*` bewertet werden.
- Task- und Timing-Integration in bestehende `commTask` reduziert Integrationsaufwand im Feld.
- `FEAT_GNSS` erlaubt weiterhin Builds ohne GNSS-Pfad (Code und Runtime-Verhalten werden deaktiviert).

## Konsequenzen

- `NavigationState` enthält GNSS-Felder (lat/lon/alt/sog/cog/fix, Zeitstempel, Qualitätsmetriken).
- GNSS-Polling läuft in `commTask` (50 Hz), Quality/Freshness analog bestehender Dependency-Policy.
- HAL stellt GNSS-UART-Schnittstellen bereit (`hal_gnss_*`).
- `gps-bridge/reference` bleibt vorerst als Referenz bestehen, ist aber nicht der produktive Hauptpfad.

## Verworfen

- Eigenständiges `gps-bridge` Firmware-Projekt mit separaten Build-Profilen und eigener Laufzeit.
  Dies wurde verworfen, weil aktuell die engere Integration in Safety/Control/Logging überwiegt.
