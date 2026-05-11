# Distributed Multiplayer Tetris

Deterministic LAN multiplayer Tetris using:
- C++20
- SDL2
- ENet
- Multithreading (`render`, `simulation`, `network`)
- Input lockstep synchronization (host-authoritative orchestration)

## Features Implemented
- Playable Tetris board rendering with SDL2
- Deterministic simulation core with fixed tick
- Host/client LAN networking with ENet
- Input-only synchronization (not full-state per frame)
- Frame-based lockstep execution
- Periodic state hash broadcast
- Separate network and simulation threads from render loop

## Controls
- Left Arrow: move left
- Right Arrow: move right
- Up Arrow / `X`: rotate clockwise
- Down Arrow: soft drop
- Space: hard drop
- `C`: hold

## Build (Windows + vcpkg recommended)

### 1) Install dependencies with vcpkg
If you already use vcpkg:

```powershell
vcpkg install sdl2 enet
```

### 2) Configure with CMake
From repo root:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 3) Build
```powershell
cmake --build build --config Release
```

Binary path (Visual Studio generator):
- `build/Release/distributed_tetris.exe`

## Run Multiplayer on LAN

### Host machine
1. Find host machine local IP:
   ```powershell
   ipconfig
   ```
   Use IPv4 address (example: `192.168.1.42`).
2. Allow UDP port in Windows Firewall (example uses `7777`):
   ```powershell
   netsh advfirewall firewall add rule name="DistributedTetris UDP 7777" dir=in action=allow protocol=UDP localport=7777
   ```
3. Run host:
   ```powershell
   .\distributed_tetris.exe --host --port 7777 --players 2
   ```

### Client machine (same LAN)
1. Copy/build the same executable on client machine.
2. Run client pointing to host IP:
   ```powershell
   .\distributed_tetris.exe --client --address 192.168.1.42 --port 7777 --players 2
   ```

### Notes
- Host auto-starts match once required players are connected.
- `--players` must match on all machines.
- Use same build version on all nodes for deterministic behavior.

## CLI Options
- `--host` : run as host
- `--client` : run as client
- `--address <ip>` : host IP (client mode)
- `--port <number>` : UDP port (default `7777`)
- `--players <n>` : expected players (default `2`, minimum `2`)
- `--tick <n>` : simulation tick rate (default `30`)

## Current Scope and Next Improvements
This version is an MVP foundation for your distributed systems project. Recommended next steps:
- Add UI text for FPS/RTT/hash/desync counters
- Add heartbeat/timeout packet handling
- Add explicit lobby join/leave packets and player-ID assignment
- Add metrics thread and log files in `/logs`
- Add experiment mode toggle for full-state sync baseline
