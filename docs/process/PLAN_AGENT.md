# PLAN_AGENT — Parallelisierungs- und Merge-Regeln

Dieses Dokument definiert, wie ein Plan-Agent Aufgaben in **unabhängige** und **abhängige** Arbeitspakete zerlegt, Risiken früh erkennt und Konflikte bei paralleler Bearbeitung vermeidet.

## Ziel

- Parallele Arbeit ermöglichen, ohne Integrationschaos zu erzeugen.
- Dateikonflikte und semantische Kollisionen früh sichtbar machen.
- Klare Regeln für Reihenfolge (`exclusive_before`, `parallelizable_after`) und Besitz von Dateien (Claiming/Locking) setzen.

---

## 1) Datei-Footprint-Analyse

Vor jeder Planung muss ein **Datei-Footprint** je Task erstellt werden.

### Pflichtinhalt pro Task

- `task_id`: Eindeutige Kennung.
- `change_intent`: Kurzbeschreibung der beabsichtigten Änderung.
- `files_read`: Dateien, die nur gelesen werden.
- `files_write`: Dateien, die geändert/erstellt/gelöscht werden.
- `public_surface`: Betroffene öffentliche Schnittstellen (z. B. Header, PGN-Schema, API-Verträge, Build-Konfiguration).
- `risk_notes`: Bekannte Integrations- oder Regression-Risiken.

### Klassifikation

- **Narrow footprint**: wenige, lokal isolierte Dateien.
- **Wide footprint**: mehrere Module, zentrale Konfigs oder öffentliche Schnittstellen.
- **Hotspot footprint**: Dateien, die häufig von mehreren Tasks berührt werden (z. B. `README.md`, zentrale Header, Index-Dateien).

---

## 2) independent vs. dependent

Jeder Task muss explizit markiert werden:

- `independent`: Kann ohne Vorbedingung gestartet werden und bricht keine erwartete Schnittstelle anderer laufender Tasks.
- `dependent`: Benötigt Vorarbeiten anderer Tasks (z. B. API-Freeze, Datenmodell, Migrationsschritt).

### Heuristik

Ein Task ist **dependent**, wenn mindestens eine Bedingung zutrifft:

1. Er konsumiert eine Schnittstelle, die im selben Zyklus noch geändert wird.
2. Er schreibt in Dateien aus `merge_risk_files` (siehe unten), die bereits von einem anderen Task geclaimt sind.
3. Sein Test-/Build-Erfolg hängt von Artefakten eines anderen offenen Tasks ab.

---

## 3) `exclusive_before`

`exclusive_before` listet Tasks auf, die **zwingend abgeschlossen** sein müssen, bevor der aktuelle Task startet.

### Regel

Wenn `exclusive_before` nicht leer ist:

- Task darf nicht parallel gestartet werden.
- Start erst nach Merge/Übernahme der referenzierten Tasks.
- Re-Check der Datei-Footprint-Analyse nach Abschluss der Vorbedingung.

Beispiel:

```yaml
exclusive_before:
  - TASK-API-001
  - TASK-SCHEMA-003
```

---

## 4) `parallelizable_after`

`parallelizable_after` listet minimale Vorbedingungen, nach deren Abschluss ein Task **parallel** zu anderen laufen darf.

### Regel

- Sobald alle IDs in `parallelizable_after` erledigt sind, darf der Task in den Parallel-Pool.
- Trotzdem bleibt Claiming/Locking auf Dateiebene verpflichtend.

Beispiel:

```yaml
parallelizable_after:
  - TASK-API-001
```

---

## 5) `merge_risk_files`

`merge_risk_files` ist die projektspezifische Liste von Dateien mit hohem Konfliktpotenzial.

### Typische Kandidaten

- `README.md`
- zentrale Build-/CI-Dateien
- globale Konfigurationsdateien
- zentrale Header/Interfaces
- Index-/Registry-Dateien

### Regel

- Änderungen an `merge_risk_files` müssen früh angekündigt und exklusiv geclaimt werden.
- Wenn möglich, Änderungsfenster bündeln (z. B. „Docs-Slot", „API-Slot“).
- Bei unvermeidbarer Parallelität: kleinste mögliche Diffs, häufiges Rebase.

---

## 6) Claiming/Locking

Claiming/Locking verhindert, dass zwei Tasks dieselbe kritische Datei gleichzeitig „besitzen".

### Claim-Lebenszyklus

1. **Claim anlegen**
   - Task meldet `files_write` + geplante Dauer.
2. **Lock prüfen**
   - Kollision mit bestehendem Claim? Dann:
     - entweder warten,
     - oder Task neu schneiden (Footprint verkleinern),
     - oder explizite Reihenfolge setzen (`exclusive_before`).
3. **Bearbeiten**
   - Nur geclaimte Schreibpfade ändern.
4. **Freigeben**
   - Nach Merge oder Abbruch Claim sofort entfernen.

### Mindestregeln

- Kein Commit gegen aktive fremde Locks in derselben Datei.
- Für `merge_risk_files` ist ein **exklusiver Lock** obligatorisch.
- Locks sind kurzlebig zu halten; lange Locks in kleinere Teilaufgaben aufteilen.

---

## 7) Empfohlenes Plan-Schema (YAML)

```yaml
task_id: TASK-XYZ-123
change_intent: "Kurzbeschreibung"
classification: independent # oder dependent
exclusive_before: []
parallelizable_after: [TASK-API-001]
files_read:
  - src/module_a.cpp
files_write:
  - docs/process/PLAN_AGENT.md
public_surface:
  - docs/process/PLAN_AGENT.md
merge_risk_files:
  - README.md
risk_notes:
  - "README-Konflikte bei parallelen Doku-Tasks möglich"
```

---

## 8) Entscheidungslogik (Kurzform)

1. Datei-Footprint analysieren.
2. `independent`/`dependent` setzen.
3. Falls nötig `exclusive_before` definieren.
4. Sonst `parallelizable_after` setzen.
5. Betroffene `merge_risk_files` claimen/locken.
6. Erst danach Implementierung starten.

Diese Reihenfolge ist verbindlich für parallele Planung und Ausführung.
