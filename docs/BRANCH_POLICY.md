# Branch Policy

## Branch Model

- Stable integration branch:
  - `main`
- Long-lived feature branches:
  - `feature/localization`
  - `feature/asset-extraction`
  - `feature/reverse-analysis`
- Short-lived experiment branches:
  - `spike/*`

## Scope Ownership

- `feature/localization`
  - Timeline and localization files only.
- `feature/asset-extraction`
  - Probe dump and model assembly pipeline only.
- `feature/reverse-analysis`
  - Hook points, symbol helpers, runtime trace export only.

Cross-scope edits require explicit note in PR description.

## Merge Flow

Always merge in this order:

1. `spike/*` -> matching `feature/*`
2. `feature/*` -> `main`

Direct `spike/*` -> `main` is not allowed.

## Evidence Requirements

Every PR must include concrete evidence.

- Model dump PR:
  - F9 or F10 runtime log excerpt
  - dump audit result
  - Blender import result summary
- Hook/symbol PR:
  - trace schema update
  - sample JSON snippet
  - hook hit proof
- Localization PR:
  - before/after sample lines
  - regression check notes

## Commit and PR Hygiene

- Keep one intent per commit.
- Avoid mixed commits across tracks.
- If submodule pointers in `deps/*` change, explain why explicitly.
- Do not commit local environments, dumps, cache, or temporary outputs.
