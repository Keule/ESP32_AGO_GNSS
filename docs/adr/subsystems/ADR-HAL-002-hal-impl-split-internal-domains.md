# ADR-HAL-002: Aufteilung der HAL-Implementierung in interne Domänenmodule

- Status: proposed
- Datum: 2026-04-23

## Kontext

Die aktuelle HAL-Implementierung bündelt mehrere fachliche Domänen in
wenigen großen Implementierungsdateien. Das erschwert Navigation,
Wartung, gezielte Reviews sowie die isolierte Fehlersuche bei Build-
und Laufzeitproblemen.

Ziel ist eine interne Reorganisation der Implementierung entlang klarer
Domänengrenzen, ohne das öffentliche HAL-Vertragsverhalten für
Aufrufer zu verändern.

## Entscheidung

Die interne HAL-Implementierung wird in folgende Domänenmodule
aufgeteilt:

- `hal_spi.cpp`
- `hal_network.cpp`
- `hal_gnss.cpp`
- `hal_ads1118.cpp`
- `hal_pin_claims.cpp`

Die Aufteilung betrifft ausschließlich interne Implementierungsdetails
(`.cpp`-Ebene) und dient der besseren Kapselung der Verantwortlichkeiten.

## Invarianten

- Die Public API in `src/hal/hal.h` bleibt unverändert.
- Es handelt sich um eine rein interne Reorganisation ohne beabsichtigte
  Verhaltensänderung der externen HAL-Schnittstelle.

## Migrationsprinzip

Die Migration erfolgt strikt zweistufig:

1. **Funktionserhaltendes Verschieben** von bestehender Logik in die
   Zielmodule (keine inhaltlichen Änderungen, nur Struktur).
2. **Duplikatabbau und interne Bereinigung** erst nach stabiler,
   nachweisbarer Funktionsgleichheit.

Damit werden Ursachen bei Regressionen klarer zuordenbar gehalten.

## Risiko- und Rollback-Strategie

### Risiken

- Build-Fehler durch unvollständige Datei-Einbindung im Build-System.
- Linker-Fehler (z. B. fehlende oder doppelte Symbole) durch falsche
  Aufteilung oder verbleibende Überschneidungen.
- Reihenfolge-/Sichtbarkeitsprobleme durch geänderte interne
  Abhängigkeiten.

### Gegenmaßnahmen

- Nach jedem Migrationsschritt vollständigen Build ausführen.
- Symbole und Zuständigkeiten pro Modul eindeutig halten
  (Single Ownership pro Funktion).
- Kleine, nachvollziehbare Commits pro Teilverschiebung verwenden.

### Rollback

Bei Build-/Linker-Problemen wird auf den letzten grünen Commit
zurückgesetzt (vollständiger Git-Revert der letzten Teilmigration oder
Wiederherstellung der vorherigen Dateistruktur), bevor weitere
Bereinigungen erfolgen.

Rollback-Regel: **Stabilität vor Strukturfortschritt**.

## Konsequenzen

### Positiv
- bessere fachliche Trennung der HAL-Verantwortlichkeiten
- schnellere Orientierung und gezieltere Reviews
- geringeres Risiko großer konfliktträchtiger Änderungen in einer Datei

### Negativ
- temporär höherer Migrationsaufwand
- zusätzlicher Koordinationsbedarf bei Build-/Linker-Integration

## Alternativen

- Beibehaltung der bestehenden, zentralisierten Implementierung
  → geringerer kurzfristiger Aufwand, aber dauerhaft schlechtere
  Wartbarkeit.
- Direkter Umbau inkl. gleichzeitiger Logikbereinigung
  → schnelleres Endziel, aber deutlich höheres Regressionsrisiko.
