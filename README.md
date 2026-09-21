# SplitBox

SplitBox is a clean-room Windows desktop manager inspired by the workflow of Roblox Manager.

## v0.2.0

SplitBox now includes a working **Split Screen** mode for two running Roblox clients.

### Current features

- RM-like dark desktop UI
- local account list and search
- Roblox process/window discovery
- two-player Roblox window selector
- borderless side-by-side layout
- borderless top/bottom layout
- restores the original Roblox window size/style when Split Screen stops
- physical keyboard and mouse discovery through Windows Raw Input
- per-player keyboard/mouse assignment
- live raw-input counters so you can verify which physical device belongs to P1/P2
- experimental background keyboard + mouse-button routing

### Important input note

The **visual split screen is functional now**.

Roblox mouse-look is focus/raw-input sensitive. SplitBox v0.2.0 can distinguish physical mice and keyboards, and the experimental router can send keyboard and mouse-button messages to the assigned Roblox window, but truly simultaneous independent mouse-look for two Roblox clients is not yet guaranteed. Doing that reliably without injecting code into Roblox requires a separate input-isolation layer.

## Use

1. Start two Roblox clients with your multi-instance tool.
2. Open SplitBox.
3. Open **Split Screen**.
4. Press **Refresh devices**.
5. Pick Roblox window, keyboard and mouse for Player 1 and Player 2.
6. Pick **Side by side** or **Top / bottom**.
7. Press **Start Split Screen**.
8. Use **Stop / Restore** to return both Roblox windows to their original positions.

## Build

Requires Visual Studio/MSVC with the C++ desktop workload.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Executable:

```
build/Release/SplitBox.exe
```

GitHub Actions builds a Windows x64 artifact on every push.

> SplitBox is an independent clean-room project and is not affiliated with Roblox Corporation or Roblox Manager.
