# Third-party notices

## Source of this modified fork

Original RA2 Engine author: **Omar El Arab**.
Source: https://github.com/omarelarab/ra2-engine
Starting revision: a8cfdccfef8592c13a6517c3fb76d834b1534027.
The upstream engine is licensed under 0BSD; its LICENSE is retained.

This Android touch-control fork is maintained by Bryle13 (shietgrupe-cpu13) with OpenAI
Codex assistance. Its gesture changes and tests are modifications to Omar's
engine, not a new engine written by this fork.

MYSOREZ/GeneralsZH-Android-Port was studied as a touch-control design reference:
https://github.com/MYSOREZ/GeneralsZH-Android-Port . No Generals source or assets
were copied. Mustafa-MS/RedAlert2-Android was compared as a separate Android
architecture: https://github.com/Mustafa-MS/RedAlert2-Android . No code from
that project is included in this native APK. Full provenance is in CREDITS.md.

RA2 Engine itself is licensed under the Zero-Clause BSD licence (0BSD, see
`LICENSE`). The engine
binaries (desktop `ra2engine` and the Android APK) also include or link
the following third-party software.

## SDL 2.30.9 (Simple DirectMedia Layer)

Linked as `SDL2.dll` (desktop) / `libSDL2.so` (Android); the Android Java glue
(`android/app/src/main/java/org/libsdl/app/*`) is SDL's.
Source: https://github.com/libsdl-org/SDL (tag `release-2.30.9`; the build
downloads the release tarball pinned by SHA-256 in `cmake/sdl2_pin.cmake`).

```
Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## stb_image_write v1.16 (Sean Barrett)

`third_party/stb_image_write.h` (PNG output for screenshots / captures).
Dual-licensed public domain (Unlicense) or MIT; used here under the MIT
alternative:

```
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## doctest (Viktor Kirilov) — tests only

`third_party/doctest.h`, compiled into the `ra2assets_tests` test binary only
(not shipped). MIT License, Copyright (c) 2016-2023 Viktor Kirilov.

## MinGW-w64 runtime (Windows build)

The Windows `ra2engine.exe` / `ra2game.exe` link the GCC runtime statically:
libstdc++ and libgcc (GPL-3.0 with the GCC Runtime Library Exception, which
allows this without imposing terms on the program) and the MinGW-w64
winpthreads library:

```
Copyright (c) 2011 mingw-w64 project

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## Android toolchain runtime

The APK ships `libc++_shared.so` from the Android NDK (LLVM libc++,
Apache License 2.0 with LLVM Exceptions).
The full LLVM libc++ license and exception notices are supplied in
`licenses/LIBCXX-LICENSE.txt` (APK asset `LIBCXX-LICENSE.txt`).

## Unofficial project and research boundary

EA has not endorsed and does not support this product.
Red Alert 2 and Yuri's Revenge names and retail content belong to their
respective rights holders. The engine license does not license retail assets.
Gagarin (r-melvin/gagarin) is consulted for behavioral research only; its GPLv3
source has not been incorporated into this release. Disclaimers and local
game-file confirmation do not establish legal clearance or proof of ownership.
