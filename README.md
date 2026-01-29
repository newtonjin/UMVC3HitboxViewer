# UMVC3 Hitbox Viewer

A DirectX 9 overlay tool for Ultimate Marvel vs. Capcom 3 that displays hitboxes and hurtboxes in real time, for training, analysis, and fun. This is a beta/alpha version, isn't by any means 100% completed. Still under development.

## Overview

UMVC3 Hitbox Viewer is a real-time hitbox/hurtbox visualizer for Ultimate Marvel vs. Capcom 3. It consists of two main components:

- **DLL**: Injected into the game process, it reads all relevant memory (characters, hitboxes, hurtboxes, camera, etc) every frame, filters and organizes the data, and writes it to a shared memory region.
- **Overlay App**: A standalone executable that automatically injects the DLL if needed, then reads the shared memory and draws the hitboxes/hurtboxes as a transparent overlay on top of the game window.

The overlay is non-intrusive, does not modify game files, and has minimal performance impact.

## How It Works

- **DLL**: Injected into the game, reads memory, filters active characters, and writes all data to shared memory.
- **Overlay App**: Reads the shared memory and draws the hitboxes/hurtboxes in real time, synchronized with the game.
- **Data Matrices**: All communication is done through a struct (`SharedOverlayData`) containing arrays of characters, each with arrays of hitboxes and hurtboxes, plus camera and frame info.

## Usage

1. Start UMVC3 normally.
2. Run the overlay app:
   ```bash
   .\build\bin\UMVC3OverlayApp.exe
   ```
   The overlay app will automatically inject the DLL into the game process if needed.
3. The overlay will appear as a transparent layer on top of the game, showing hitboxes and hurtboxes in real time.

## Data Structure (Matrices)

- `SharedOverlayData` contains:
  - Frame counter, camera, zoom, etc.
  - Array of up to 6 characters (`characters[]`)
    - Each character: position, status, arrays of hitboxes and hurtboxes (up to 32 each)
    - Each hitbox/hurtbox: position (x, y, z), radius, active flag
- Everything is updated every frame for perfect sync.

## Features

- Real-time hitbox visualization (red circles)
- Real-time hurtbox visualization (translucent green circles)
- Smart filtering to avoid ghosts (frozen or offscreen characters)
- External overlay, no game file modification
- Minimal performance impact

## Memory Offsets and Data Mapping

The DLL reads the following data from UMVC3's memory using static offsets (for game version 1.0.6):

| Data                | Offset (from base/ptr) | Used In                | Description                                  |
|---------------------|------------------------|------------------------|----------------------------------------------|
| Character Root Ptr  | 0x140D44A70            | dllmain.cpp            | Main pointer to all character objects        |
| Team Roots          | +0x58, +0x328          | dllmain.cpp            | Pointers to each team's character list       |
| Char State/Tag      | +0x14                   | dllmain.cpp, character.cpp | Tag state (active, assist, etc)         |
| Action ID           | +0x1310                 | dllmain.cpp, character.cpp | Current action/move ID                   |
| Position X/Y/Z      | +0x50/+0x54/+0x58       | dllmain.cpp, character.cpp | World position (float)                    |
| Velocity X          | +0x58                   | dllmain.cpp, character.cpp | Used for filtering ghosts                  |
| Animation Frame     | +0x18                   | dllmain.cpp            | Used for heartbeat/freeze detection         |
| Hitbox Array Ptr    | (varies, see char)      | character.cpp          | Pointer to hitbox array for character       |
| Hurtbox Array Ptr   | (varies, see char)      | character.cpp          | Pointer to hurtbox array for character      |
| Camera Ptr          | 0x140E17930             | dllmain.cpp            | Camera position, zoom, etc                  |

All offsets are hardcoded for the Steam version 1.0.6. If you use another version, offsets may need updating.

### What is Pulled from Memory

- For each character (up to 6):
  - Tag state (active, assist, tagging, etc)
  - Action ID (current move)
  - Position (X, Y, Z)
  - Velocity (X)
  - Animation frame (for freeze/ghost detection)
  - Arrays of hitboxes and hurtboxes (each with position, radius, active flag)
- Camera position and zoom
- Frame counter

### Where Each Value is Used

- Filtering logic (dllmain.cpp):
  - Tag state, Action ID, Velocity, Animation frame: used to decide if a character/assist is real or a ghost
  - Position: used for on-screen checks and rendering
- Rendering (overlay_app.cpp):
  - All hitbox/hurtbox data, character positions, and camera info are used to draw the overlay
- Shared memory struct (shared_overlay_data.h):
  - All values are packed into `SharedOverlayData` and its arrays for fast access by the overlay

### Matrix/Struct Layout

```c
typedef struct SharedHitbox {
    float x, y, z;
    float radius;
    int active;
} SharedHitbox;

typedef struct SharedCharacter {
    float posX, posY, posZ;
    float altPosX, altPosY;
    int isActive, isOnScreen;
    uint32_t charIndex;
    SharedHitbox hitboxes[32];
    uint32_t hitboxCount;
    SharedHitbox hurtboxes[32];
    uint32_t hurtboxCount;
    // ...other fields
} SharedCharacter;

typedef struct SharedOverlayData {
    uint64_t frameNumber;
    float cameraX, cameraY, cameraZoom;
    uint32_t characterCount;
    SharedCharacter characters[6];
    // ...projectiles, etc
} SharedOverlayData;
```

All arrays are fixed size for performance and simplicity. The DLL fills these every frame, and the overlay app reads and renders them.


## Project Structure

```
UMVC3 Hitbox Viewer/
├── src/                  # C++ source code for DLL and overlay app
│   ├── dllmain.cpp       # Main DLL logic, memory reading, filtering, shared memory writing
│   ├── overlay_app.cpp   # Overlay renderer and injector logic
│   └── ...               # Other helpers (character, camera, etc)
├── include/              # Shared memory struct and headers
│   └── shared_overlay_data.h
├── utils/                # TrainingTools infrastructure (trampoline, memory, etc)
├── gui/                  # ImGui (not used by DLL)
├── build/                # Binaries and objects
└── README.md
```

### How Memory Reading Works

The DLL, once injected, hooks into the game's rendering loop. Every frame, it:

1. Locates the root pointer for all characters (using static offsets for the current UMVC3 version).
2. Iterates through all characters (up to 6), reading their state, position, action, and arrays of hitboxes/hurtboxes directly from game memory.
3. Reads camera position and other relevant data.
4. Applies filtering logic to avoid rendering ghosts, offscreen, or inactive characters.
5. Packs all this data into a single struct (`SharedOverlayData`) in shared memory.

The overlay app reads this struct every frame and renders the data.

### Data Matrices Explained

- `SharedOverlayData` is a C struct containing:
  - Frame counter, camera info, etc.
  - Array of up to 6 characters (`characters[]`)
    - Each character: position, state, arrays of hitboxes and hurtboxes (up to 32 each)
    - Each hitbox/hurtbox: position (x, y, z), radius, active flag
- All arrays are fixed-size for performance and simplicity.
- The DLL updates this struct every frame, so the overlay always has the latest data.

This matrix-like structure allows the overlay to efficiently access and render all hitboxes/hurtboxes for all characters, every frame, with no lag.

## Building

### Requirements
- Visual Studio 2022 with C++ workload
- Git (for cloning)
- ~500MB free disk space

### Steps
```powershell
cd "UMVC3 modding"
.\build_vscode.bat
```
Output:
- `build\bin\UMVC3Overlay.dll`
- `build\bin\UMVC3OverlayApp.exe`

Build time: ~5-10 seconds on modern hardware.

## Troubleshooting

**Overlay does not appear:**
- Make sure the DLL was injected (check UMVC3Overlay.log)
- Start the overlay app after the game is running
- Try running the overlay app as Administrator

**Missing hitboxes/hurtboxes:**
- Check if the game memory offsets are correct in `dllmain.cpp` (UpdateSharedMemory)
- Make sure the game is in Training mode
- Verify character count > 0 in shared memory

**Game crashes:**
- The DLL is stable and only reads memory
- If crashes occur, check if the game process is still running

**Overlay app crashes:**
- Make sure the DLL is running (check log file updates)
- Shared memory may not be populated yet
- Wait 2-3 seconds after DLL injection

**No hitboxes/hurtboxes visible:**
- Overlay window must be on top of the game

## Known Issues

- Some hitboxes/hurtboxes may appear slightly misaligned (game offsets are not always exact)
- Not all projectiles are rendered correctly
- X-23 may not appear rendered in some situations
- Overlay may not work with non-standard game executables (hardcoded offsets)
- Overlay only works in windowed/borderless mode

## Technical Details

- **Hook Address**: 0x140289c5a (DirectX Present)
- **Shared Memory Name**: L"UMVC3OverlayData"
- **Max Characters**: 6
- **Max Hitboxes/Hurtboxes per char**: 32
- **Framework**: TrainingTools (Trampoline, MemoryMgr)

## Debugging

Check `UMVC3Overlay.log` in the mod directory for detailed logs:
```
[UMVC3] InitThread: Starting...
[UMVC3] InitThread: Hook installation complete!
[UMVC3] PresentHook: Frame 120
```

## Legal Notice

This mod is for **educational and training purposes only**. Use at your own risk on your copy of UMVC3. The authors are not responsible for any issues or bans.

---

## Shout Outs

Special thanks to the **Marvel Modding Discord** and the entire UMVC3 modding community for their support, resources, and inspiration.

A huge shout out to **EternalYoshi**, whose research, tools, and documentation served as the foundation for much of the work in this project. Without his contributions, this tool would not have been possible.

If you're interested in UMVC3 modding, join the Marvel Modding Discord they do know their shit.
