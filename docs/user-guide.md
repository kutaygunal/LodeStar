# Lodestar — End-User Guide

Lodestar is a GNSS/SBAS Integrated Test & Verification (IT&V) platform. It
automates the full IT&V lifecycle — from scenario generation to
certification-ready reporting — with built-in DO-178C / ARP4754A / ARP4761
compliance, FMEA/hazard analysis, and cross-disciplinary integration.

This guide covers installation, running the application, and the main features.

---

## 1. System requirements

- **Operating system:** Windows 10 / 11 (64-bit)
- **Processor:** x86-64, 4+ cores recommended
- **Memory:** 8 GB RAM minimum, 16 GB recommended
- **Disk:** 2 GB free space
- **Display:** 1280×720 or higher

## 2. Installation

### 2.1 Install from the packaged bundle

1. Obtain the Lodestar installer bundle (a ZIP archive or the staged
   `build/package` directory produced by `packaging/installer.ps1`).
2. Extract the bundle to a folder of your choice, for example
   `C:\Program Files\Lodestar`.
3. Ensure the folder contains `lodestar_app.exe` together with the Qt runtime
   DLLs (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`) and the `platforms`
   subfolder. These are required for the application to start.

### 2.2 Install via the packaging script

From a PowerShell prompt in the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File packaging/installer.ps1 `
    -BuildDir build/ui/Release `
    -InstallDir "C:\Program Files\Lodestar"
```

This stages the app + DLLs, copies the license and documentation, installs to
the target directory, and produces a distributable ZIP.

## 3. Running the application

Launch the application by double-clicking `lodestar_app.exe`, or from a
terminal:

```powershell
.\lodestar_app.exe
```

For a headless/offscreen run (e.g. on a server or in CI):

```powershell
.\lodestar_app.exe --platform offscreen
```

On first launch the application creates and migrates its local database
(`lodestar_app.db`) automatically. No manual setup is required.

## 4. Main features

### 4.1 TraceLink — Systems Engineering
- Link requirements, design, interfaces, and test cases for full traceability.
- Interactive traceability matrix, impact analysis, and baseline diff views.
- Suspect-link detection and change-request workflow.

### 4.2 TestForge — IT&V Plans
- Auto-generate, execute, and report test procedures with data analysis.
- Equivalence-class and boundary-value test-case design from requirements.
- Structural coverage (statement/decision/MC/DC) and certification reporting.

### 4.3 AssureCheck — Assurance Standards
- Automated compliance checks against ARP4754A, ARP4761, DO-178C, DO-278A,
  and DO-254.
- Review/approval/sign-off workflow with audit trail and evidence packages.

### 4.4 ScenarioForge — GNSS / SBAS
- Generate realistic GPS/Galileo/SBAS RF and data scenarios for test injection.

### 4.5 RiskAI — Risk & Safety
- LLM-assisted hazard assessments and FMEA with risk-mitigation tracking.

### 4.6 IntegrateHub — Cross-Disciplinary
- Central hub connecting software, hardware, RF, electrical, and mission-ops
  data and issue resolution.

### 4.7 Dashboard & Reporting
- Coverage dashboard with charts, compliance dashboards, and export to
  HTML/CSV/JSON/PDF/ReQIF.

## 5. Getting help

See `docs/support.md` for support tiers and contact information.
