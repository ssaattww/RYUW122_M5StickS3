# README user guide implementation report

## Metadata

- Repository: `ssaattww/RYUW122_M5StickS3`
- Pull request: #8
- Branch: `codex/readme-user-guide`
- Base branch: `master`
- Base HEAD: `c3c801ba8207387fb2654602ed60f4cfeae90abd`
- README implementation HEAD: `ad04043380f8b0229fea9691e6104c36e924a07e`
- Report type: implementation report
- Merge boundary: merge is left to the repository owner

## Purpose

Create the repository root `README.md` as an operator-facing guide for the current M5StickS3 + RYUW122 firmware.
The requested content is:

- M5StickS3 and RYUW122 wiring
- NT-Shell settings and commands for node ID, TAG/ANCHOR mode, and ANCHOR position
- TAG BtnA page switching and TAG position graph interpretation
- TAG/ANCHOR broadcast-list interpretation
- status/state/error interpretation
- runtime behavior explanation
- links to the existing sequence diagrams instead of duplicating them

## Scope and non-goals

### In scope

- Add `README.md`.
- Document only behavior that is present in the current source and accepted design documents.
- Cross-check electrical wiring against the REYAX and M5Stack hardware documentation.

### Non-goals

- No firmware behavior change.
- No NVS key or command change.
- No display-layout change.
- No CI workflow change because diagnostic artifact capture already exists.
- No `tasks-status.md` update because the existing tracked task set is complete and this request is a standalone documentation change.
- No merge.

## Diagnostic artifact workflow check

Before editing, `.github/workflows/ci.yml` was inspected.
The existing `PlatformIO CI` workflow already captures the required failure diagnostics and uploads them with `actions/upload-artifact@v4` under an `if: always()` step.
Captured data includes:

- test/build standard output (`*.stdout.log`)
- test/build standard error (`*.stderr.log`)
- command and exit-code result files (`*.result.txt`)
- Git revision
- PlatformIO version and system information
- setup standard output, standard error, and result

No workflow modification was required.

## Sources inspected

Repository sources and design documents:

- `.github/workflows/ci.yml`
- `platformio.ini`
- `include/BuildOptions.h`
- `include/ConfigPreference.h`
- `include/RunMode.h`
- `include/NodeStatus.h`
- `include/SequentialRangingDisplay.h`
- `include/SequentialRangingController.h`
- `include/TagMasterCoordinator.h`
- `include/NtpTimeProtocolCodec.h`
- `src/main.cpp`
- `src/ConfigPreference.cpp`
- `src/ConfigRuntime.cpp`
- `src/PreferenceCommands.cpp`
- `src/NtShell.cpp`
- `src/Ryuw122Controller.cpp`
- `src/Ryuw122Initializer.cpp`
- `src/EspNowBroadcast.cpp`
- `src/RangingDisplayTaskController.cpp`
- `src/SequentialRangingDisplay.cpp`
- `src/TagPositionInput.cpp`
- `src/TagPositionViewController.cpp`
- `src/TagPositionGraphRenderer.cpp`
- `docs/preferences-commands.md`
- `docs/feature-list.md`
- `docs/sequential-ranging-time-sync.md`

External primary hardware references:

- REYAX `RYUW122_Lite` datasheet
- REYAX `RYUW122` datasheet
- M5Stack StickS3 hardware documentation / Hat2-Bus pin map

## Implemented README content

### Wiring

The README documents the 6-pin `RYUW122_Lite` connection used by the firmware:

- StickS3 `3V3_L2` -> RYUW122_Lite `VDD`
- StickS3 `GND` -> RYUW122_Lite `GND`
- StickS3 `G7` -> RYUW122_Lite `RXD`
- StickS3 `G1` <- RYUW122_Lite `TXD`
- StickS3 `G8` -> RYUW122_Lite `NRST`
- RYUW122_Lite `PA7` unused

The README also records the REYAX VDD range of 2.4-3.6 V and explicitly warns against connecting the module VDD to StickS3 `EXT_5V` or Grove 5 V.

### NT-Shell and NVS setup

The README records that the normal `m5stack-sticks3` build has NT-Shell enabled continuously on USB serial at 115200 bps, with `SH` shown in the status bar.
It documents the `pref` command set and the required per-node settings:

- `node_id` (`u8`)
- `run_mode` (`u8`, `0=TAG`, `1=ANCHOR`)
- `anchor_pos_x` (`u16`, mm)
- `anchor_pos_y` (`u16`, mm)

It includes copyable TAG and ANCHOR examples, notes that node IDs must be unique across roles, and notes that NVS changes require a reboot because `ConfigRuntime` loads the values during startup.
It also documents the shared ESP-NOW channel and optional Wi-Fi power-save setting.

### Runtime behavior

The README summarizes the implemented startup and ranging sequence:

1. load NVS configuration
2. reset and initialize RYUW122
3. start ESP-NOW and NodeStatus broadcast
4. elect the minimum-ID active TAG as master
5. perform three-sample NTP four-timestamp synchronization and select the minimum-RTT valid sample
6. perform sequential ranging with ANCHOR ID as the outer loop and TAG ID as the inner loop
7. return each result immediately to the master and forward follower-TAG results to the target follower
8. use only each TAG's own measurements for its list and position estimate

The existing sequence diagrams are linked from `docs/sequential-ranging-time-sync.md#14-シーケンス図` instead of being copied into the README.

### Display guide

The README explains:

- status bar fields (`ID`, `TAG/ANCHOR`, `SH`, battery percentage)
- role display (`M`, `F`, `A`, `?`)
- state display (`WAIT`, `FOLLOW`, `SYNC`, `READY`, `RUN`, `IDLE`, `RANGE`)
- time-quality display (`SYNC`, `PWR`, `RX?`, `OLD`, `UNSYNC`)
- `NOW`, `OK LAST`, and `CURRENT FAIL`
- TAG BtnA switching between the ranging-list page and position graph
- position fields `X`, `Y`, used ANCHOR count, and residual RMS
- white ANCHOR markers and red TAG marker
- insufficient-anchor, degenerate-geometry, and invalid-input messages
- common `ID MODE X,Y` NodeStatus broadcast list
- current display snapshot limit of five measurement/node entries
- startup failure strings and `TASK START FAILED`

## Development and validation policy

This change is documentation-only. No executable behavior was added or changed, so a Red/Green TDD cycle is not applicable.
The implementation was validated by source/design cross-check and GitHub diff inspection.

### Diff inspection

`master...codex/readme-user-guide` at the README implementation HEAD showed:

- 1 commit ahead
- 0 commits behind
- only `README.md` added
- 321 additions
- no implementation, test, configuration, workflow, or tracking-file changes

### GitHub Actions

For README implementation HEAD `ad04043380f8b0229fea9691e6104c36e924a07e`:

- Workflow: `PlatformIO CI`
- Run ID: `31930159311`
- Job: `test-and-build`
- Job head SHA: `ad04043380f8b0229fea9691e6104c36e924a07e`
- State when this report was generated: `in_progress`
- Completed setup steps at that point: checkout, Python setup, PlatformIO installation
- Native tests and firmware builds were still running

This run is recorded only because its `head_sha` exactly matches the README implementation HEAD. No run for another SHA is used as CI evidence.
The report persistence commit changes the PR HEAD after this report is created; the matching run for that final PR HEAD must therefore be checked separately and recorded in the PR completion comment.

## Intentionally untouched areas

- Firmware source: unchanged because the request is documentation-only.
- Tests: unchanged because no executable contract changed.
- `.github/workflows/ci.yml`: unchanged because failure diagnostic artifact capture already satisfies the project requirement.
- `tasks-status.md`: unchanged because the tracked task list is already complete and the file states that it is updated only through the designated task-management flow.

## Remaining risks

- The wiring is validated against firmware pin definitions and primary hardware datasheets, but no physical wiring test was performed in this work session.
- Screen explanations are validated against the current renderer/controller source; no new on-device screenshot was captured.
- README position examples are illustrative values and are not measurements from a physical deployment.

## Next action

- Persist this report on the PR branch.
- Check the PR's new current HEAD after the report commit.
- Use only a GitHub Actions run whose `head_sha` exactly matches that new PR HEAD.
- Post a concise PR comment containing the final HEAD and matching CI state.
- Do not merge; the repository owner performs the merge.

## Handoff packet

```yaml
schema_version: 3
repository: ssaattww/RYUW122_M5StickS3
mode: implementation
branch: codex/readme-user-guide
base_ref: master
base_head: c3c801ba8207387fb2654602ed60f4cfeae90abd
implementation_head: ad04043380f8b0229fea9691e6104c36e924a07e
pull_request: 8
scope:
  - add operator-facing README
  - document wiring and NT-Shell configuration
  - document runtime behavior and display interpretation
  - link existing sequence diagrams
non_goals:
  - firmware changes
  - test changes
  - workflow changes
  - task tracking changes
  - merge
files:
  changed:
    - path: README.md
      purpose: operator hardware/configuration/operation guide
  report:
    - path: reports/readme-user-guide-implementation.md
validation:
  development_method: documentation-only; TDD not applicable
  implementation_ci:
    run_id: 31930159311
    head_sha: ad04043380f8b0229fea9691e6104c36e924a07e
    status_at_report_generation: in_progress
  required_final_check: check only the workflow run whose head_sha equals the PR current HEAD after report persistence
failure_diagnostics:
  workflow: .github/workflows/ci.yml
  artifact_upload: already present with always() execution
remaining_risks:
  - no physical hardware wiring test in this session
  - no new on-device display screenshot in this session
next_action:
  type: report
  summary: persist report, resolve new PR HEAD, check matching CI, post concise PR comment
merge: forbidden_for_worker
```
