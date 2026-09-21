# SplitBox

SplitBox is a clean-room Windows desktop manager inspired by the workflow of Roblox Manager.

Current goals:
- RM-like dark desktop UI
- local account list and presets
- Roblox process/window discovery
- multi-instance friendly window management foundation
- future Split Screen tab with per-player keyboard/mouse assignment

## Build

Requires Visual Studio 2022 / MSVC with C++ desktop workload.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable will be at:

```
build/Release/SplitBox.exe
```

GitHub Actions also builds a Windows x64 artifact on every push.

> SplitBox is an independent clean-room project and is not affiliated with Roblox Corporation or Roblox Manager.
