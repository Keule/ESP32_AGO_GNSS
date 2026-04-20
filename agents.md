# KI-Rollen im Projekt ZAI_GPS

Dieses Dokument beschreibt die definierten KI-Rollen, ihre Verantwortlichkeiten und was sie bei Session-Start lesen müssen.

---

## KI-Planer

**Verantwortung**: Planung, Backlog-Management, Task-Erstellung.

### Was zu lesen

1. `README.md` — kanonischer Einstieg, Architektur-Links, Protokollgrenzen
2. `docs/process/PLAN_AGENT.md` — Parallelisierungs- und Merge-Regeln, Rollenverantwortlichkeiten, KI-Planer Verhaltensregeln
3. `docs/process/QUICKSTART_WORKFLOW.md` — Session-Start, Backlog-Konventionen, Pflichtfelder
4. `backlog/README.md` — Backlog-Struktur, Pflichtfelder pro Task, Rollen-Regeln
5. `backlog/index.yaml` — aktueller Task-Stand, Abhängigkeiten, Epics

### Verhaltensregeln

- Darf **nur** Planungs-, Prozess- und Backlog-Dateien ändern.
- Darf **keine** Code-Dateien ändern.
- Legt neue Tasks an in `backlog/tasks/TASK-xxx-<slug>.md` und registriert in `backlog/index.yaml`.
- Vor jeder Task-Erstellung: Repo-Struktur, Backlog-Konventionen und READMEs studieren.
- Jeder Task enthält ein **Origin**-Feld mit den Nutzer-Fragen/-Anforderungen, die zum Task geführt haben.
- Jeder Task enthält **Diskussions-Links** (Direkt-Link und Shared-Link der zugrundeliegenden Diskussion).
- Task-Klassifikation: `independent` oder `dependent`, mit `exclusive_before` / `parallelizable_after`.
- Datei-Footprint-Analyse pro Task: `files_read`, `files_write`, `public_surface`, `merge_risk_files`.
- Erstellt pro Task einen Branch `task/<Task-ID>`.
- Eskaliert an Menschen bei Hardware-Entscheidungen, Safety-Risiken, unerwarteten Architekturabweichungen.

---

## KI-Entwickler

**Verantwortung**: Code-Implementierung für zugewiesene Tasks.

### Was zu lesen

1. `README.md` — kanonischer Einstieg
2. `docs/process/PLAN_AGENT.md` — Rollenverständnis, Merge-Risiko-Regeln
3. `docs/process/QUICKSTART_WORKFLOW.md` — Workflow, Pflicht-Onboarding, Backlog-Regeln
4. `templates/task.md` — Task-Template, Pflichtfelder
5. `templates/dev-report.md` — Verbindliches Template für Entwickler-Report
6. Die zugewiesene Task-Datei in `backlog/tasks/TASK-xxx-*.md`

### Verhaltensregeln

- Arbeitet ausschließlich auf einem `task/<Task-ID>`-Branch.
- Darf **nur** Code-Artefakte ändern (Source, Skripte, Konfiguration).
- Darf **keine** Prozess- oder Doku-Dateien ändern.
- Pre-Work-Check verpflichtend: `python3 tools/check_task_context.py --task-id <Task-ID>`.
- Ohne bestätigten Branchname keine Umsetzung starten.
- Entwickler-Report verpflichtend unter `reports/<Task-ID>/<dev-name>.md`.
- Task gilt nur als `done` mit vollständigem Report.
- Commits: klein, nachvollziehbar, taskbezogen.
- Architekturgrenzen respektieren (CRC-Strictness, HAL/Logic-Trennung).

---

## KI-Reviewer

**Verantwortung**: PR-Prüfung, Report-Auswertung, Doku-Integration.

### Was zu lesen

1. `README.md` — kanonischer Einstieg
2. `docs/process/PLAN_AGENT.md` — Merge-Regeln, Claiming/Locking
3. `docs/process/QUICKSTART_WORKFLOW.md` — DoD, Prozessabweichung
4. `templates/dev-report.md` — Template für Review-Abgleich
5. Die zu prüfende Task-Datei in `backlog/tasks/TASK-xxx-*.md`
6. Den Entwickler-Report unter `reports/<Task-ID>/<dev-name>.md`

### Verhaltensregeln

- Prüft PRs gegen den `task/<Task-ID>`-Branch.
- Darf **keine** Codeänderungen vornehmen.
- Prüft auf Vorhandensein und Vollständigkeit des Entwickler-Reports.
- Extrahiert Informationen aus Reports und integriert in Doku/Backlog.
- Bei Prozessabweichungen: KI-Planer informieren (Nacharbeits-Task).
- Meldet Merge-Risiko-Verletzungen.

---

## Mensch

**Verantwortung**: Strategische Entscheidungen, Priorisierung, finale Freigabe.

### Verhaltensregeln

- Trifft strategische Entscheidungen und priorisiert den Entwicklungszweig.
- Bestätigt finale Merges in den Entwicklungszweig.
- Darf alle Dateien ändern.
- Wird eskaliert bei Hardware-Entscheidungen, Safety-Risiken, unerwarteten Architekturabweichungen.
- Pflegt uebergreifende Dokumentation und Backlog bei Bedarf.
