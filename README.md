# SplitBox

SplitBox is a clean-room Windows desktop manager inspired by the workflow of Roblox Manager.

## v0.3.0 — openMouse integration

SplitBox now uses **openMouse** as its multi-pointer input engine instead of trying to invent its own mouse multiplexer.

openMouse upstream:
https://github.com/alstonmendonca/openMouse

SplitBox pins openMouse to commit:

`dee7810ec23aac5de13c544a57e9e519b176062c`

The upstream project is MIT licensed.

## What works now

- RM-like dark desktop UI
- local account list and search
- Roblox process/window discovery
- two-player Roblox window selector
- borderless side-by-side layout
- borderless top/bottom layout
- restores original Roblox window size/style when split screen stops
- bundles `openmouse.exe` next to `SplitBox.exe`
- SplitBox can start/stop openMouse automatically
- **Identify devices** button launches `openmouse.exe --identify`
- **Probe** launches `openmouse.exe --probe`
- **Probe echo** launches `openmouse.exe --probe-echo`
- **openMouse config** opens `%APPDATA%\openMouse`
- optional openMouse keyboard steering toggle
- openMouse provides independent mouse pointers, click ownership, scrolling and dragging for separate physical mice

## First setup

Do this once on a machine before trusting the split-screen input mapping:

1. Start SplitBox.
2. Open **Split Screen**.
3. Click **Probe**.
4. Click **Probe echo**.
5. Click **Identify devices**.
6. Follow openMouse's console prompts:
   - move only Player 1's mouse when asked
   - type only on Player 1's keyboard when asked
   - repeat for Player 2
7. openMouse stores the binding in:
   `%APPDATA%\openMouse\openmouse.ini`

## Playing

1. Launch two Roblox clients with your multi-instance launcher.
2. Open **Split Screen** in SplitBox.
3. Click **Refresh devices**.
4. Pick the Roblox window for Player 1.
5. Pick the Roblox window for Player 2.
6. Select **Side by side** or **Top / bottom**.
7. Optionally enable **openMouse keyboard steering**.
8. Click **Start Split Screen**.

SplitBox will:

- start openMouse if it is not already running
- make the two Roblox windows borderless
- place them in the selected split layout
- let openMouse own the physical multi-pointer mouse layer

Press **Stop / Restore** to stop openMouse and restore the Roblox windows.

### Emergency shortcut

While openMouse is running:

`Ctrl + Alt + Shift + Q`

releases input and exits openMouse.

## Important Roblox limitation

openMouse solves the **Windows multi-pointer problem**, which is much better than SplitBox's old experimental mouse routing.

However, Roblox may use raw/relative mouse input for locked camera movement. openMouse itself documents that Windows still has one foreground window, and it was designed primarily around normal desktop pointer behavior.

So:

- two independent pointers/clicks: handled by openMouse
- per-seat scrolling/dragging: handled by openMouse
- keyboard: steering only
- two fully independent Roblox raw mouse-look cameras at the exact same instant: **not guaranteed yet**

This is a real Windows/Roblox input limitation, not something SplitBox should hide behind a fake "working" checkbox.

## Build

Requires Visual Studio/MSVC with the C++ desktop workload.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

CMake fetches the pinned openMouse source automatically.

Build output:

```
build/bin/SplitBox.exe
build/bin/openmouse.exe
```

GitHub Actions packages both executables together.

> SplitBox is an independent clean-room project and is not affiliated with Roblox Corporation or Roblox Manager.
