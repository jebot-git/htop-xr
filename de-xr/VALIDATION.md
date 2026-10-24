# Validation — 24 October 2026

Release build against upstream `394b4a3f` and installed FLTK 1.4.5.

- Upstream collector regression passed: live CPU/memory, disposable child
  discovery, Unicode case folding, PID ordering and pidfd STOP/CONT/TERM.
- Launcher checks passed: byte-identical upstream help/version, interactive
  console launch under a pseudo-terminal, explicit/automatic GUI dispatch,
  and untouched htoprc. The staged TUI is byte-identical to the release build.
- Eleven isolated native Wayland checks passed on DomeD/Xvfb: desktop and XR
  process tables, private matching floor files, BDF-labelled coloured bars,
  multiple-window ownership, withdrawal on clean exit, app identity and
  universal-menu protocol export. Captures were visually reviewed.
- DomeD's 104 release library tests passed, including four performance-floor
  tests for expiry/disabled/future/invalid values, frame size and serial,
  play-area placement, and bounded private-file reads.
- Real WiVRn / Meta Quest Pro run: 120 performance-floor frames submitted,
  two texture uploads, one flat STAGE layer, clean exit. The GPU-rendered
  1024×256 floor canvas was captured. This confirms runtime submission;
  headset readability and comfort remain subject to in-headset user review.

Reproducible tests and build commands are in README.md. Local logs, screenshots
and the OpenXR report are retained under build/ and are excluded from git.

## Undecorated XR projection

The follow-up Wayland run passed 15 checks, including transparent canvas corners,
transparent tile grout and transparent unused CPU/RAM/SWAP tracks. The composited
preview was visually checked; desktop meter styling is unchanged.
