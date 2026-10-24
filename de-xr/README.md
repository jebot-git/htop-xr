# htop for de-xr

An FLTK/OpenGL process viewer built from upstream htop's collectors. The source
snapshot is upstream main `394b4a3f` (19 October 2026); see `UPSTREAM.json` for
provenance. The existing htop C sources and console executable are unchanged.
This Linux frontend is maintained as a de-xr fork, not an upstream htop release.

## Launching

- `htop` in a terminal runs upstream's TUI, with its normal keys and configuration.
- `htop --tui [upstream options]` explicitly selects the TUI.
- `htop --gui` or the **htop** application menu entry opens the native Wayland GUI.
- A graphical launch without terminal descriptors also opens the GUI. Arguments
  such as `--help`, `--version`, `-p` and `-u` select upstream's CLI unless `--gui`
  is explicitly first. The GUI never reads, migrates or writes your htoprc.

The GUI has CPU, memory and swap meters, task/load readouts, a searchable process
table, clickable sorting headers and stable PID selection. F5 refreshes; F9
requests termination; Ctrl+Q closes. The universal **Process** menu also offers
Stop and Continue, with confirmation. Actions pin the selected process with a
Linux pidfd, so a recycled PID cannot receive the signal. Unsupported kernels
report an error rather than falling back to unsafe PID signalling.

Appearance follows DomeD's live palette, bitmap font family, script, size and
scale settings. The shared cmuxr UI helpers retain their MIT notices. GL widget
rendering uses the installed patched FLTK; floor canvases use the same BDF text
rasterizer. Arbitrary UTF-8 process names are supported; invalid process bytes
are replaced for display. Long commands are clipped before texture allocation.

## XR performance floor

On DomeD's XR Wayland socket the process list remains a window; CPU/RAM/SWAP bars
and their labels are projected directly onto the STAGE floor, with transparent
space between the tiles and no panel background or empty bar tracks. On desktop
they remain in the window. The **XR floor readouts** checkbox can disable the projection.

DomeD renders a 4:1 strip inside the forward edge of the play rectangle, at most
2.8 m wide. It shares cmuxr's STAGE coordinates and runtime or configured
`cmuxr/settings.json` `floor_size_m` bounds. It is an independent layer above
cmuxr's spectrum and does not take input focus. There is no inferred LOCAL-space
floor. Locked scenes and fullscreen XR media suppress both floor overlays.
Changes to DomeD's renderer become effective on the next XR renderer launch;
installation does not restart an active desktop or display-manager session.

The GUI publishes measured samples once per second into the private runtime
directory. `htop-floor.json` (version 1) contains UNIX timestamp, enabled flag,
PID, serial, canvas dimensions and CPU/RAM/SWAP ratios. `htop-floor.rgba` contains
`HTPF`, the matching little-endian u64 serial, then exactly 1024×256×4 top-down
RGBA bytes. Both are atomically replaced with mode 0600. A singleton advisory
lock lets extra GUI windows coexist without overwriting the floor. Pause and
clean exit withdraw it; a crash expires within 2.5 seconds. DomeD bounds reads,
rejects symlinks/nonregular/public files, and requires a matching frame serial.

## Release build and installation

Required: a C/C++ toolchain, autotools, CMake, Ninja, ncurses development files,
libcap/libunwind development files, GLib/GIO, Wayland, X11, Fontconfig, zlib, and
de-xr's installed FLTK 1.4.5 toolkit with BDF support. No Qt frontend is built.

Run as a regular user:

```sh
./de-xr/build.sh
ctest --test-dir build/gui --output-on-failure
python3 de-xr/test-launcher.py
python3 de-xr/test-gui.py  # Xvfb, Pillow and installed DomeD required
./de-xr/stage.sh
```

Review `build/stage`, then install system files with the native sudo prompt:

```sh
SUDO_ASKPASS=/usr/local/bin/dome-askpass sudo -A ./de-xr/install.sh
```

Files install under `/usr/local`. Existing managed targets are backed up under
`/var/backups/htop-de-xr/UNIX_TIME`. Distribution packages are left intact. The
upstream `./autogen.sh && ./configure && make` build remains a console-only build.
The frontend's out-of-tree build separately archives upstream collector objects
without `htop.o`; its C adapter initializes metadata without starting curses.

`--dump-json` samples the real collector without a display. `--screenshot
FILE.ppm` captures two GUI samples for QA and exits. GUI test artifacts are in
`build/gui-artifacts`; the collector regression uses only its own disposable
child for STOP/CONT/TERM tests. Test and build artifacts are not committed.
