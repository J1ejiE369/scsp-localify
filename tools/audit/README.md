# Audit Scripts Workspace

Place ad-hoc validation scripts here instead of repository root.

## Naming Convention

- `check_*` for quick one-shot checks
- `audit_*` for structured validation reports
- `inspect_*` for deeper diagnostics

## Rules

- Scripts should read input paths from arguments when possible.
- Keep outputs in temporary folders or ignored locations.
- Do not commit generated dumps or large binary artifacts.
