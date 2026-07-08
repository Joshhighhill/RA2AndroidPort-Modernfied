# ra2assets

A portable C++17 library that decodes Red Alert 2 game archives (MIX/PAL/SHP).

## Building

Prerequisites: CMake 3.20+, MinGW-w64 gcc, Ninja, Python 3.13

Configure and build with CMake + Ninja:
```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Run tests:
```bash
ctest --test-dir build --output-on-failure
```

## Preflight Check

Verify the game install and binary before RE/oracle work:
```bash
RA2_INSTALL_DIR="D:\Red Alert 2" python scripts/preflight.py
```

The script checks that `gamemd.exe` exists and matches the expected SHA1 hash for YR 1.001.
