# Workstream Status

## Objective

Turn the current mixed workspace into a traceable, parallel-safe workflow across three tracks:

1. Localization delivery
2. Runtime model dump (character and later scene)
3. Hook and symbol tracing for reverse analysis

## Current Repository Snapshot

- Repo: `J1ejiE369/scsp-localify` (`origin`)
- Remote branches:
  - `main` -> `b5d3611`
  - `feature/reverse-analysis` -> `4e5645a`
  - `feature/asset-extraction` -> `0896199`
  - `feature/localization` -> `b5d3611`
  - `feature/timeline-dump` -> `034f1b4`
  - `feature/timeline-hook-v2` -> `f7fa355`
- Local active branch: `feature/reverse-analysis`
- Legacy branch:
  - `feature/model-dumper-legacy` (kept for history, no active development)

## Workstream Breakdown

### A) Localization Track

- Goal: maintain a stable, near-release localization line.
- Branch target: `feature/localization`.
- Scope:
  - Timeline and text replacement logic
  - Localization resources and regression checks
- Current status:
  - Existing mature work is closest to `feature/timeline-hook-v2`.

### B) Runtime Model Dump Track

- Goal: export runtime-assembled character/scene data reliably.
- Branch target: `feature/asset-extraction`.
- Scope:
  - `src/probe/probe.cpp`
  - `src/probe/probe.hpp`
  - `Model_Extraction_Tools/*` (excluding local virtual environments)
- Current blocker:
  - Hair meshes often fail readable extraction path at runtime.

### C) Hook Trace and Symbol Track

- Goal: create high-signal runtime traces and symbol exports to guide hook placement.
- Branch target: `feature/reverse-analysis`.
- Scope:
  - `src/hook.cpp`
  - `src/il2cpp/il2cpp_symbols.cpp`
  - `src/il2cpp/il2cpp_symbols.hpp`
- Current need:
  - Build an F10 trace JSON path to reconstruct loading and mesh-read chain.

## Working Tree Inventory (Before Split)

- Modified:
  - `deps/minhook` (submodule pointer changed)
  - `deps/rapidjson` (submodule pointer changed)
  - `deps/safetyhook` (submodule pointer changed)
  - `src/hook.cpp`
  - `src/il2cpp/il2cpp_symbols.cpp`
  - `src/il2cpp/il2cpp_symbols.hpp`
- Untracked:
  - `src/probe/*`
  - `Model_Extraction_Tools/*`

## Noise and Risk Notes

- `Model_Extraction_Tools/venv/*` is local environment noise and should not enter Git history.
- Submodule pointer changes in `deps/*` need explicit confirmation before any commit.
- Root-level ad-hoc scripts in workspace (outside repo) should be consolidated under `tools/audit/`.

## Next Actions

1. Keep daily development constrained to the three active branches.
2. Treat `feature/model-dumper-legacy` as read-only history.
3. Keep rescue snapshots until asset/localization tracks are fully validated.
4. Enforce PR evidence requirements from `BRANCH_POLICY.md`.

## Experiment Record Format

Use this minimal record for each trial:

- `experiment_id`:
- `branch`:
- `input_sample`:
- `changes`:
- `result` (`pass` or `fail`):
- `evidence_files`:
- `next_step`:
